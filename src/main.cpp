#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <csignal>

#include "iptv/manifest/playlist_m3u8_parser.hpp"
#include "iptv/network/http_client.hpp"
#include "iptv/network/http_init.hpp"
#include "iptv/player/playback_orchestrator.hpp"
#include "iptv/demux/ts_demuxer.hpp"
#include "iptv/decoder/ffmpeg_decoder.hpp"
#include "iptv/decoder/ffmpeg_audio_decoder.hpp"
#include "iptv/concurrency/concurrent_queue.hpp"

using namespace iptv::network;
using namespace iptv::manifest;
using namespace iptv::decoder;
using namespace iptv::demux;
using namespace iptv;
using namespace iptv::player;
constexpr size_t kMaxQueueSize = 64;
constexpr int kMetricsReportIntervalMs = 1000;

static std::atomic<bool> g_stop_flag{false};

static void signalHandler(int /*signum*/) {
    g_stop_flag.store(true, std::memory_order_relaxed);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <url_m3u8>\n";
        std::cerr << "Example: " << argv[0] << " https://test-streams.mux.dev/x36xhzz/x36xhzz.m3u8\n";
        return 1;
    }

    std::string url = argv[1];
    std::cout << "🎬 VSMN-Player CLI Validator\n";
    std::cout << "------------------------------\n";
    std::cout << "Descargando: " << url << "...\n";

    // Init libcurl and subsystems
    iptv::network::initialize();

    // Create HTTP client with hard timeouts to prevent stalled transfers
    HttpClient client;
    NetworkConfig config{};
    config.connect_timeout  = std::chrono::seconds(5);
    config.timeout          = std::chrono::seconds(30);
    config.ssl_verify       = false;  // OK for test streams with self-signed certs
    client.setNetworkConfig(config);

    // Anti-stall safeguards: low speed limit so stalled transfers abort after 3s
    // (configured in http_client.cpp: CURLOPT_LOW_SPEED_LIMIT 10240, LOW_SPEED_TIME 3)

    auto network_component = std::make_unique<NetworkComponent>(std::make_unique<HttpClient>(std::move(client)));

    // Create playback orchestrator - wires network layer with playback queue
    PlaybackOrchestrator orchestrator(std::move(network_component));

    // Start orchestrator background thread: downloads manifest, selects ABR variant,
    // and feeds MediaSegmentBundle into the shared concurrent queue
    orchestrator.start(url);

    // Get reference to the thread-safe concurrent queue containing downloaded segments
    auto& queue = orchestrator.getQueue();

    // Create demuxer and decoders
    TsDemuxer demuxer;
    // Video decoder with empty hint auto-detects codec from first bitstream
    FFmpegDecoder video_decoder("");
    // Audio decoder requires a non-empty codec hint; AAC is typical for HLS streams
    FFmpegAudioDecoder audio_decoder("aac");

    // Metrics tracking
    int total_decoded_frames = 0;
    int frames_last_interval = 0;
    auto last_metrics_time = std::chrono::steady_clock::now();

    // Consumer thread: pops segments from queue, demuxes, decodes, and logs metrics
    std::jthread consumer([&queue, &demuxer, &video_decoder, &audio_decoder,
                           &total_decoded_frames, &frames_last_interval,
                           &last_metrics_time](std::stop_token stop_token) {
        while (!g_stop_flag.load(std::memory_order_acquire) && !stop_token.stop_requested()) {
            // Blocking pop: waits until an item is available or stop is requested
            auto bundle = queue.pop(stop_token);

            if (!bundle.has_value()) {
                // Queue empty + stop requested, or cancellation
                if (stop_token.stop_requested()) {
                    break;
                }
                std::this_thread::yield();
                continue;
            }

            // Demux the MPEG-TS buffer: extract video/audio packets and their stream indices
            int video_idx = -1, audio_idx = -1;
            std::vector<DemuxedPacket> packets =
                demuxer.demux(std::span<const uint8_t>(bundle.value().raw_buffer),
                              &video_idx, &audio_idx);

            // Decode video packets
            for (const auto& pkt : packets) {
                if (pkt.stream_index == video_idx && video_idx >= 0) {
                    auto frames = video_decoder.decode(
                        std::span<const uint8_t>(pkt.data), pkt.pts, pkt.dts);
                    frames_last_interval += static_cast<int>(frames.size());
                    // Discard raw frames (no render path); frame data is freed when frames go out of scope
                    for (const auto& frame : frames) {
                        (void)frame;
                    }
                }
            }

            // Decode audio packets
            for (const auto& pkt : packets) {
                if (pkt.stream_index == audio_idx && audio_idx >= 0) {
                    auto frames = audio_decoder.decode(
                        std::span<const uint8_t>(pkt.data), pkt.pts, pkt.dts);
                    frames_last_interval += static_cast<int>(frames.size());
                    // Discard raw frames (no render path); audio data is freed when frames go out of scope
                    for (const auto& frame : frames) {
                        (void)frame;
                    }
                }
            }

            total_decoded_frames += frames_last_interval;

            // Report real-time metrics at regular intervals
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - last_metrics_time).count();
            if (elapsed_ms >= kMetricsReportIntervalMs) {
                double fps = 0.0;
                if (elapsed_ms > 0) {
                    fps = static_cast<double>(frames_last_interval) /
                          (static_cast<double>(elapsed_ms) / 1000.0);
                }
                size_t current_queue_depth = queue.size();
                std::cout << "\r[" << elapsed_ms << "ms] "
                          << "FPS: " << fps << " | "
                          << "Decoded: " << total_decoded_frames << " | "
                          << "Queue: " << current_queue_depth << std::flush;

                frames_last_interval = 0;
                last_metrics_time = now;
            }
        }
    });

    // Wait for the stop signal (SIGINT). The consumer thread and orchestrator
    // will both cooperate via their stop tokens; main just waits for the user signal.
    while (!g_stop_flag.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Graceful shutdown: signal the orchestrator to stop its background thread
    orchestrator.stop();

    // Print final metrics and cleanup
    std::cout << "\n--- Final Metrics ---\n";
    std::cout << "Total decoded frames: " << total_decoded_frames << "\n";
    std::cout << "Queue depth at shutdown: " << queue.size() << "\n";
    std::cout << "Graceful shutdown complete.\n";

    // Note: consumer jthread will be joined on destruction (cooperative stop via stop_token).
    // Orchestrator::~PlaybackOrchestrator joins worker_thread_ and clears the queue.

    iptv::network::shutdown();
    return 0;
}