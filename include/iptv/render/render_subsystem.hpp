#pragma once

#include <memory>

#include "iptv/common/types.hpp"

namespace iptv::render {

/**
 * @brief Subsystem responsible for presenting video and audio to the user.
 *
 * Abstraction layer over SDL2, OpenGL, or platform-specific APIs.
 */
class IRenderSubsystem {
 public:
  virtual ~IRenderSubsystem() = default;

  /**
   * @brief Initializes the rendering window and audio devices.
   */
  virtual bool initialize() = 0;

  /**
   * @brief Queues a decoded video frame for presentation.
   *
   * @param frame Decoded video frame. Handled by value to enforce move semantics.
   */
  virtual void queueVideoFrame(VideoFrame frame) = 0;

  /**
   * @brief Queues a decoded audio frame for presentation.
   *
   * @param frame Decoded audio frame. Handled by value to enforce move semantics.
   */
  virtual void queueAudioFrame(AudioFrame frame) = 0;

  /**
   * @brief Drops all queued frames (e.g. during a seek operation).
   */
  virtual void flush() = 0;
};

/**
 * @brief Factory for creating the default SDL2-based Render Subsystem.
 */
std::shared_ptr<IRenderSubsystem> createDefaultRenderSubsystem();

}  // namespace iptv::render
