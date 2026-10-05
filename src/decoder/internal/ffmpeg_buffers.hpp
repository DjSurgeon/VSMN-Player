#pragma once

#include <memory>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace iptv::decoder::internal {

/**
 * @brief Time base every frame time this decoder reports is expressed in.
 *
 * The packets a decoder is fed carry no timestamps, so it pins its context to this base and
 * synthesizes frame times from the rate the codec reports. Milliseconds keep those values in a
 * scale a render path can use directly.
 */
inline constexpr AVRational kFrameTimeBase{1, 1000};

/**
 * @brief Ownership of the FFmpeg objects a decoder holds for its whole lifetime.
 *
 * Every one of them is a raw C handle that leaks unless it is released, so each is wrapped at the
 * point it is acquired and never appears raw past this header.
 */
struct AVCodecContextDeleter {
  void operator()(AVCodecContext* context) const noexcept {
    if (context != nullptr) {
      avcodec_free_context(&context);
    }
  }
};

struct AVFrameDeleter {
  void operator()(AVFrame* frame) const noexcept {
    if (frame != nullptr) {
      av_frame_free(&frame);
    }
  }
};

struct AVPacketDeleter {
  void operator()(AVPacket* packet) const noexcept {
    if (packet != nullptr) {
      av_packet_free(&packet);
    }
  }
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

/**
 * @brief Locates the decoder named by @p codec_hint and opens it.
 * @param codec_hint The FFmpeg codec name, e.g. "h264".
 * @return The opened context, owned by the caller.
 * @throws std::runtime_error when the codec is unknown, its context cannot be allocated, or it
 *         cannot be opened.
 */
[[nodiscard]] CodecContextPtr openCodecContext(const std::string& codec_hint);

/**
 * @brief Allocates the frame buffer a decoder reuses for every picture.
 * @throws std::runtime_error when the allocation fails.
 */
[[nodiscard]] FramePtr allocateFrameBuffer();

/**
 * @brief Allocates the packet buffer a decoder reuses for every packet.
 * @throws std::runtime_error when the allocation fails.
 */
[[nodiscard]] PacketPtr allocatePacketBuffer();

}  // namespace iptv::decoder::internal