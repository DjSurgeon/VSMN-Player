#include "iptv/player/playback_orchestrator.hpp"

namespace iptv::player {

PlaybackOrchestrator::PlaybackOrchestrator(
    std::unique_ptr<network::NetworkComponent> network_component)
    : network_(std::move(network_component)) {}

PlaybackOrchestrator::~PlaybackOrchestrator() { stop(); }

void PlaybackOrchestrator::start(const std::string& master_playlist_url) {
  // If a thread is already running, stop it first
  stop();

  // Start the background thread
  worker_thread_ = std::jthread([this, url = master_playlist_url](std::stop_token stop_token) {
    downloadLoop(std::move(stop_token), url);
  });
}

void PlaybackOrchestrator::stop() {
  if (worker_thread_.joinable()) {
    worker_thread_.request_stop();
    worker_thread_.join();
  }
  segment_queue_.clear();
}

void PlaybackOrchestrator::downloadLoop(std::stop_token stop_token, const std::string& master_url) {
  // 1. Download Master Playlist
  auto master_res = network_->downloadPlaylist(master_url);
  if (!std::holds_alternative<network::ParsedPlaylistBundle>(master_res)) {
    return;  // Fast exit if cancelled or HTTP error
  }

  auto& master_bundle = std::get<network::ParsedPlaylistBundle>(master_res);

  // Safety check: is it really a master playlist?
  if (master_bundle.playlist.type != manifest::PlaylistType::Master ||
      master_bundle.playlist.variants.empty()) {
    return;
  }

  // 2. Initial Variant Selection
  double current_throughput = 0.0;
  std::string current_variant_uri(
      abr_manager_.selectVariant(master_bundle.playlist.variants, current_throughput).uri);

  uint64_t next_sequence_index = 0;

  while (!stop_token.stop_requested()) {
    // 3. Download Variant Playlist
    auto variant_res = network_->downloadPlaylist(current_variant_uri);
    if (!std::holds_alternative<network::ParsedPlaylistBundle>(variant_res)) {
      break;
    }

    auto& variant_bundle = std::get<network::ParsedPlaylistBundle>(variant_res);

    // 4. Download Segments
    for (const auto& segment : variant_bundle.playlist.segments) {
      if (stop_token.stop_requested()) {
        return;
      }

      // Skip segments we've already downloaded
      if (segment.sequence_index < next_sequence_index) {
        continue;
      }

      auto segment_res = network_->downloadSegment(segment, stop_token);
      if (!std::holds_alternative<network::MediaSegmentBundle>(segment_res)) {
        break;  // Retry at playlist level on hard failures
      }

      auto& media_bundle = std::get<network::MediaSegmentBundle>(segment_res);
      current_throughput = media_bundle.metrics.throughputMbps();

      // Update sequence tracker for continuity
      next_sequence_index = segment.sequence_index + 1;

      // Push to queue for the Demuxer thread
      segment_queue_.push(std::move(media_bundle));

      // ABR Check: Post-segment analysis (Opción A)
      const auto& ideal_variant =
          abr_manager_.selectVariant(master_bundle.playlist.variants, current_throughput);

      if (ideal_variant.uri != current_variant_uri) {
        // ABR decided to switch quality.
        // Flush queue to avoid playing old low/high res segments.
        segment_queue_.clear();
        current_variant_uri = std::string(ideal_variant.uri);

        // Break segment loop to fetch the new variant's playlist
        break;
      }
    }

    if (variant_bundle.playlist.has_endlist) {
      // VOD Stream finished successfully
      break;
    }
  }
}

}  // namespace iptv::player
