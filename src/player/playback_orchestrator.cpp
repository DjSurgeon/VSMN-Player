#include "iptv/player/playback_orchestrator.hpp"

namespace iptv::player {

namespace {

std::string resolveUrl(std::string_view base_url, std::string_view uri) {
  if (uri.starts_with("http://") || uri.starts_with("https://")) {
    return std::string(uri);
  }
  size_t last_slash = base_url.find_last_of('/');
  if (last_slash == std::string_view::npos) {
    return std::string(uri);
  }
  if (uri.starts_with('/')) {
    size_t host_end = base_url.find('/', base_url.find("://") + 3);
    if (host_end == std::string_view::npos) {
      return std::string(base_url) + std::string(uri);
    }
    return std::string(base_url.substr(0, host_end)) + std::string(uri);
  }
  return std::string(base_url.substr(0, last_slash + 1)) + std::string(uri);
}

}  // namespace

PlaybackOrchestrator::PlaybackOrchestrator(
    std::unique_ptr<network::NetworkComponent> network_component)
    : network_(std::move(network_component)) {}

PlaybackOrchestrator::~PlaybackOrchestrator() { stop(); }

void PlaybackOrchestrator::start(const std::string& master_playlist_url) {
  // If a thread is already running, stop it first
  stop();

  // Start the background thread
  worker_thread_ = std::jthread([this, url = master_playlist_url](std::stop_token stop_token) {
    downloadLoop(stop_token, url);
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
  std::string initial_uri =
      resolveUrl(master_url,
                 abr_manager_.selectVariant(master_opt->playlist.variants, current_throughput).uri);

  PlaybackContext ctx{.master_url = master_url,
                      .master_playlist = master_opt->playlist,
                      .current_variant_uri = initial_uri,
                      .next_sequence_index = 0,
                      .current_throughput = 0.0};

  processVariantLoop(stop_token, ctx);
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
                                              PlaybackContext& ctx) {
  while (!stop_token.stop_requested()) {
    auto variant_res = network_->downloadPlaylist(ctx.current_variant_uri);
    if (!std::holds_alternative<network::ParsedPlaylistBundle>(variant_res)) {
      break;
    }

    auto variant_bundle = std::get<network::ParsedPlaylistBundle>(std::move(variant_res));

    bool should_continue = downloadSegments(stop_token, ctx, variant_bundle.playlist);

    if (!should_continue || variant_bundle.playlist.has_endlist) {
      break;
    }
  }
}

/**
 * @brief Downloads all pending segments from the current variant.
 */
bool PlaybackOrchestrator::downloadSegments(const std::stop_token& stop_token, PlaybackContext& ctx,
                                            const manifest::Playlist& variant_playlist) {
  for (const auto& segment : variant_playlist.segments) {
    if (stop_token.stop_requested()) {
      return false;
    }

    if (segment.sequence_index < ctx.next_sequence_index) {
      continue;
    }

    std::string absolute_uri = resolveUrl(ctx.current_variant_uri, segment.uri);
    auto segment_res = network_->downloadSegment(absolute_uri, stop_token);
    if (!std::holds_alternative<network::MediaSegmentBundle>(segment_res)) {
      return false;  // Hard failure
    }

    auto media_bundle = std::get<network::MediaSegmentBundle>(std::move(segment_res));
    ctx.current_throughput = media_bundle.metrics.throughputMbps();
    ctx.next_sequence_index = segment.sequence_index + 1;

    segment_queue_.push(std::move(media_bundle));

    if (evaluateAbrSwitch(ctx)) {
      return true;  // Broke loop intentionally to switch variant
    }
  }
  return true;
}

/**
 * @brief Checks with ABR manager if a quality switch is required.
 */
bool PlaybackOrchestrator::evaluateAbrSwitch(PlaybackContext& ctx) {
  const auto& ideal_variant =
      abr_manager_.selectVariant(ctx.master_playlist.variants, ctx.current_throughput);
  std::string next_uri = resolveUrl(ctx.master_url, ideal_variant.uri);

  if (next_uri != ctx.current_variant_uri) {
    segment_queue_.clear();
    ctx.current_variant_uri = std::move(next_uri);
    return true;
  }
  return false;
}

}  // namespace iptv::player
