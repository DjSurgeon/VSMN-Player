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

/**
 * @brief Main entry point for the background download thread.
 */
void PlaybackOrchestrator::downloadLoop(const std::stop_token& stop_token,
                                        const std::string& master_url) {
  auto master_opt = fetchMasterPlaylist(master_url);
  if (!master_opt) {
    return;
  }

  double current_throughput = 0.0;
  std::string initial_uri(
      abr_manager_.selectVariant(master_opt->playlist.variants, current_throughput).uri);

  processVariantLoop(stop_token, master_opt->playlist, initial_uri);
}

/**
 * @brief Downloads and validates the master playlist.
 */
std::optional<network::ParsedPlaylistBundle> PlaybackOrchestrator::fetchMasterPlaylist(
    const std::string& master_url) {
  auto master_res = network_->downloadPlaylist(master_url);
  if (!std::holds_alternative<network::ParsedPlaylistBundle>(master_res)) {
    return std::nullopt;
  }

  auto master_bundle = std::get<network::ParsedPlaylistBundle>(std::move(master_res));

  if (master_bundle.playlist.type != manifest::PlaylistType::Master ||
      master_bundle.playlist.variants.empty()) {
    return std::nullopt;
  }

  return master_bundle;
}

/**
 * @brief Core loop for fetching variants and iterating over their segments.
 */
void PlaybackOrchestrator::processVariantLoop(const std::stop_token& stop_token,
                                              const manifest::Playlist& master_playlist,
                                              const std::string& initial_variant_uri) {
  uint64_t next_sequence_index = 0;
  double current_throughput = 0.0;
  std::string current_variant_uri = initial_variant_uri;

  while (!stop_token.stop_requested()) {
    auto variant_res = network_->downloadPlaylist(current_variant_uri);
    if (!std::holds_alternative<network::ParsedPlaylistBundle>(variant_res)) {
      break;
    }

    auto variant_bundle = std::get<network::ParsedPlaylistBundle>(std::move(variant_res));

    bool should_continue =
        downloadSegments(stop_token, master_playlist, variant_bundle.playlist, current_variant_uri,
                         next_sequence_index, current_throughput);

    if (!should_continue || variant_bundle.playlist.has_endlist) {
      break;
    }
  }
}

/**
 * @brief Downloads all pending segments from the current variant.
 */
bool PlaybackOrchestrator::downloadSegments(const std::stop_token& stop_token,
                                            const manifest::Playlist& master_playlist,
                                            const manifest::Playlist& variant_playlist,
                                            std::string& current_variant_uri,
                                            uint64_t& next_sequence_index,
                                            double& current_throughput) {
  for (const auto& segment : variant_playlist.segments) {
    if (stop_token.stop_requested()) {
      return false;
    }

    if (segment.sequence_index < next_sequence_index) {
      continue;
    }

    auto segment_res = network_->downloadSegment(segment, stop_token);
    if (!std::holds_alternative<network::MediaSegmentBundle>(segment_res)) {
      return false;  // Hard failure
    }

    auto media_bundle = std::get<network::MediaSegmentBundle>(std::move(segment_res));
    current_throughput = media_bundle.metrics.throughputMbps();
    next_sequence_index = segment.sequence_index + 1;

    segment_queue_.push(std::move(media_bundle));

    if (evaluateAbrSwitch(master_playlist, current_variant_uri, current_throughput)) {
      return true;  // Broke loop intentionally to switch variant
    }
  }
  return true;
}

/**
 * @brief Checks with ABR manager if a quality switch is required.
 */
bool PlaybackOrchestrator::evaluateAbrSwitch(const manifest::Playlist& master_playlist,
                                             std::string& current_variant_uri, double throughput) {
  const auto& ideal_variant = abr_manager_.selectVariant(master_playlist.variants, throughput);

  if (ideal_variant.uri != current_variant_uri) {
    segment_queue_.clear();
    current_variant_uri = std::string(ideal_variant.uri);
    return true;
  }
  return false;
}

}  // namespace iptv::player
