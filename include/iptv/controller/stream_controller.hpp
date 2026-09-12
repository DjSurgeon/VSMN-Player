#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "iptv/common/types.hpp"
#include "iptv/decoder/decoder_subsystem.hpp"
#include "iptv/network/network_subsystem.hpp"
#include "iptv/render/render_subsystem.hpp"

namespace iptv::controller {

/**
 * @brief The Playback Orchestrator (Cerebro of the Player).
 *
 * Responsible for integrating Network, Decoder, and Render subsystems.
 * Implements Active Resilience features like Dual-Watermark Buffering, EWMA-based
 * Adaptive Bitrate selection, and Fallback to Audio-Only.
 */
class StreamController {
 public:
  /**
   * @brief Constructs the orchestrator with its dependencies injected.
   */
  StreamController(std::shared_ptr<network::INetworkSubsystem> network,
                   std::shared_ptr<decoder::IDecoderSubsystem> decoder,
                   std::shared_ptr<render::IRenderSubsystem> render);

  ~StreamController();

  // Delete copy/move to enforce singleton-like lifecycle per playback session
  StreamController(const StreamController&) = delete;
  StreamController& operator=(const StreamController&) = delete;

  /**
   * @brief Starts the playback of an IPTV stream or VOD URL.
   */
  void play(std::string_view playlist_url);

  /**
   * @brief Pauses or resumes playback.
   */
  void togglePause();

  /**
   * @brief Stops playback and cleans up resources.
   */
  void stop();

  /**
   * @brief Queries the current playback state.
   */
  PlaybackState getState() const;

 private:
  std::shared_ptr<network::INetworkSubsystem> network_;
  std::shared_ptr<decoder::IDecoderSubsystem> decoder_;
  std::shared_ptr<render::IRenderSubsystem> render_;

  PlaybackState current_state_{PlaybackState::Stopped};

  // Internal threading and orchestration details will go here in the future
};

}  // namespace iptv::controller
