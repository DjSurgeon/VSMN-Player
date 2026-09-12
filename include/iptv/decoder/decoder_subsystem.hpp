#pragma once

#include <memory>
#include <optional>
#include <stop_token>

#include "iptv/common/types.hpp"

namespace iptv::decoder {

/**
 * @brief Represents the status of a decode operation.
 */
enum class DecodeResult {
  Success,        ///< Packet decoded into one or more frames successfully.
  NeedsMoreData,  ///< Packet consumed, but a full frame is not yet ready.
  Error,          ///< Fatal decoding error.
  Eof             ///< End of stream reached.
};

/**
 * @brief Subsystem responsible for demuxing and decoding raw media packets.
 *
 * Abstraction layer over FFmpeg (libavformat, libavcodec).
 */
class IDecoderSubsystem {
 public:
  virtual ~IDecoderSubsystem() = default;

  /**
   * @brief Feeds a raw packet from the network into the decoder pipeline.
   *
   * @param packet The raw H.264/AAC/MPEG-TS packet.
   * @return Status of the operation.
   */
  virtual DecodeResult sendPacket(const Packet& packet) = 0;

  /**
   * @brief Extracts a fully decoded frame (if available).
   *
   * @return A DecodedFrame (variant) ready to be rendered, or nullopt if `NeedsMoreData`.
   */
  virtual std::optional<DecodedFrame> receiveFrame() = 0;

  /**
   * @brief Flushes internal FFmpeg buffers, required after a seek or discontinuity.
   */
  virtual void flush() = 0;
};

/**
 * @brief Factory for creating the default FFmpeg-based Decoder Subsystem.
 */
std::shared_ptr<IDecoderSubsystem> createDefaultDecoderSubsystem();

}  // namespace iptv::decoder
