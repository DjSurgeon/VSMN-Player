#include <gtest/gtest.h>
#include <iptv/decoder/ffmpeg_decoder.hpp>
#include <iptv/decoder/ffmpeg_audio_decoder.hpp>
#include <iptv/decoder/decoder_error.hpp>
#include <vector>
#include <thread>
#include <atomic>

using namespace iptv::decoder;

TEST(DecoderStressTest, ConcurrentDecoding) {
    constexpr int kNumThreads = 8;
    constexpr int kIterations = 100;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < kIterations; ++i) {
            try {
                // Initialize decoders concurrently to test FFmpeg's thread safety
                FFmpegDecoder video_dec("h264");
                FFmpegAudioDecoder audio_dec("aac");

                // Decode some empty/dummy data to stress the error handling/allocation paths
                std::vector<uint8_t> dummy_data(100, 0);
                
                try {
                    video_dec.decode(dummy_data);
                } catch (const DecoderException&) {}

                try {
                    audio_dec.decode(dummy_data);
                } catch (const DecoderException&) {}

                success_count++;
            } catch (const DecoderException&) {
                // Ignore initialization failures in dummy context, just want to check memory leaks/crashes
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count, 0);
}
