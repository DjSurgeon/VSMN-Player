#pragma once

#include <functional>
#include <memory>
#include <stop_token>
#include <string_view>

#include "iptv/common/types.hpp"
#include "iptv/manifest/playlist.hpp"
#include "iptv/manifest/playlist_parse_result.hpp"

namespace iptv::network {

/**
 * @brief EWMA (Exponentially Weighted Moving Average) Network Metrics.
 */
struct NetworkMetrics {
  double throughput_mbps{0.0};  ///< Current smoothed bandwidth estimation
  double latency_ms{0.0};       ///< Round-trip time estimation
};

/**
 * @brief Subsystem responsible for downloading manifests, streams, and A/V segments.
 */
class INetworkSubsystem {
 public:
  virtual ~INetworkSubsystem() = default;

  /**
   * @brief Downloads and parses the master or media playlist from a URL.
   *
   * @param url The HTTP/HTTPS URI of the playlist (.m3u8).
   * @param st Stop token to abort the download early.
   * @return Parsed Playlist or an error representation.
   */
  virtual manifest::ParseResult downloadPlaylist(std::string_view url, std::stop_token st) = 0;

  /**
   * @brief Downloads a raw media segment (e.g. .ts or .m4s).
   *
   * @param segment Metadata containing the URI and duration.
   * @param st Stop token to abort the download (e.g. for Fast Downswitch).
   * @return A Packet containing the raw byte stream of the segment.
   */
  virtual std::optional<Packet> downloadSegment(const manifest::MediaSegmentRef& segment,
                                                std::stop_token st) = 0;

  /**
   * @brief Returns real-time EWMA metrics of the active HTTP connections.
   */
  virtual NetworkMetrics getMetrics() const = 0;

  /**
   * @brief Registers a callback to be notified when network metrics change significantly.
   */
  virtual void onMetricsChanged(std::function<void(const NetworkMetrics&)> callback) = 0;
};

/**
 * @brief Factory for creating the default curl-based Network Subsystem.
 */
std::shared_ptr<INetworkSubsystem> createDefaultNetworkSubsystem();

}  // namespace iptv::network
