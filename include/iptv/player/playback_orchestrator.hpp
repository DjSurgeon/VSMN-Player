#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "iptv/abr/abr_manager.hpp"
#include "iptv/common/concurrent_queue.hpp"
#include "iptv/network/network_component.hpp"

namespace iptv::player {

/**
 * @brief Encapsulates the mutable and immutable state of the background playback loop.
 */
struct PlaybackContext {
  const std::string& master_url;
  const manifest::Playlist& master_playlist;
  std::string current_variant_uri;
  uint64_t next_sequence_index{0};
  double current_throughput{0.0};
};

/**
 * @brief Main orchestrator bridging the network layer with the playback queue.
 *
 * Manages the background download loop, ABR (Adaptive Bitrate) decision making,
 * and feeds media segments into a thread-safe concurrent queue for the demuxer.
 */
class PlaybackOrchestrator {
 public:
  /**
   * @brief Constructs a new Playback Orchestrator.
   * @param network_component Unique pointer to the configured network component for HTTP
   * interactions.
   */
  explicit PlaybackOrchestrator(std::unique_ptr<network::NetworkComponent> network_component);

  /**
   * @brief Destroys the Playback Orchestrator, ensuring the background thread is safely stopped.
   */
  ~PlaybackOrchestrator();

  PlaybackOrchestrator(const PlaybackOrchestrator&) = delete;
  PlaybackOrchestrator& operator=(const PlaybackOrchestrator&) = delete;
  PlaybackOrchestrator(PlaybackOrchestrator&&) = delete;
  PlaybackOrchestrator& operator=(PlaybackOrchestrator&&) = delete;

  /**
   * @brief Starts the background network thread to download the manifest and media segments.
   * @param master_playlist_url The URL of the master or media HLS stream to play.
   */
  void start(const std::string& master_playlist_url);

  /**
   * @brief Stops the background network thread via stop_token injection and clears the segment
   * queue.
   */
  void stop();

  /**
   * @brief Accesses the thread-safe concurrent queue for the demuxer/decoder.
   * @return A reference to the concurrent queue containing downloaded MediaSegmentBundles.
   */
  iptv::ConcurrentQueue<network::MediaSegmentBundle>& getQueue() { return segment_queue_; }

 private:
  /**
   * @brief Background loop executed by the worker thread to continuously fetch segments.
   * @param stop_token The cooperative cancellation token injected by std::jthread.
   * @param master_url The URL of the master or media HLS stream.
   */
  void downloadLoop(const std::stop_token& stop_token, const std::string& master_url);

  /**
   * @brief Fetches and validates the master playlist.
   * @param master_url The URL of the master playlist.
   * @return An optional containing the parsed bundle if successful and valid.
   */
  std::optional<network::ParsedPlaylistBundle> fetchMasterPlaylist(const std::string& master_url);

  /**
   * @brief Manages the infinite loop for fetching variant playlists and segments.
   * @param stop_token Cooperative cancellation token.
   * @param ctx The active playback context state.
   */
  void processVariantLoop(const std::stop_token& stop_token, PlaybackContext& ctx);

  /**
   * @brief Iterates and downloads segments for the current variant.
   * @param stop_token Cooperative cancellation token.
   * @param ctx The active playback context state.
   * @param variant_playlist The active variant playlist containing the segments.
   * @return true if the loop should continue, false if a hard error occurred.
   */
  bool downloadSegments(const std::stop_token& stop_token, PlaybackContext& ctx,
                        const manifest::Playlist& variant_playlist);

  /**
   * @brief Evaluates if an ABR switch is necessary based on recent throughput.
   * @param ctx The active playback context state.
   * @return true if the variant switched, false otherwise.
   */
  bool evaluateAbrSwitch(PlaybackContext& ctx);

  std::unique_ptr<network::NetworkComponent> network_;
  abr::AbrManager abr_manager_;
  iptv::ConcurrentQueue<network::MediaSegmentBundle> segment_queue_;
  std::jthread worker_thread_;
};

}  // namespace iptv::player
