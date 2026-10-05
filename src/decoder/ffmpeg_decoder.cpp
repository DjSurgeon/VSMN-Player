#include "iptv/decoder/ffmpeg_decoder.hpp"

#include <stdexcept>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace iptv::decoder {

namespace {

// Custom deleters for FFmpeg C structs to ensure zero leaks.
struct AVCodecContextDeleter {
  void operator()(AVCodecContext* ctx) const noexcept {
    if (ctx) {
      avcodec_free_context(&ctx);
    }
  }
};

struct AVFrameDeleter {
  void operator()(AVFrame* frame) const noexcept {
    if (frame) {
      av_frame_free(&frame);
    }
  }
};

struct AVPacketDeleter {
  void operator()(AVPacket* pkt) const noexcept {
    if (pkt) {
      av_packet_free(&pkt);
    }
  }
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

/**
 * @brief Locates the decoder named by @p codec_hint and opens it.
 * @throws std::runtime_error when the codec is unknown, its context cannot be allocated,
 *         or it cannot be opened. The caller receives ownership of the context on success.
 */
CodecContextPtr openCodecContext(const std::string& codec_hint) {
  const AVCodec* codec = avcodec_find_decoder_by_name(codec_hint.c_str());
  if (codec == nullptr) {
    throw std::runtime_error("FFmpeg codec not found: '" + codec_hint +
                             "'. Supply a decoder name supported by this FFmpeg build.");
  }

  CodecContextPtr context(avcodec_alloc_context3(codec));
  if (!context) {
    throw std::runtime_error("Failed to allocate AVCodecContext for codec '" + codec_hint +
                             "'. The system is out of memory.");
  }

  if (avcodec_open2(context.get(), codec, nullptr) < 0) {
    throw std::runtime_error("Failed to open FFmpeg codec '" + codec_hint +
                             "'. Verify the codec is supported by this FFmpeg build.");
  }

  return context;
}

/**
 * @brief Allocates the reusable frame buffer.
 * @throws std::runtime_error when the allocation fails.
 */
FramePtr allocateFrameBuffer() {
  FramePtr frame(av_frame_alloc());
  if (!frame) {
    throw std::runtime_error("Failed to allocate FFmpeg AVFrame. The system is out of memory.");
  }
  return frame;
}

/**
 * @brief Allocates the reusable packet buffer.
 * @throws std::runtime_error when the allocation fails.
 */
PacketPtr allocatePacketBuffer() {
  PacketPtr packet(av_packet_alloc());
  if (!packet) {
    throw std::runtime_error("Failed to allocate FFmpeg AVPacket. The system is out of memory.");
  }
  return packet;
}

}  // namespace

struct FFmpegDecoder::Impl {
  std::string codec_name;
  CodecContextPtr codec_context;
  FramePtr frame;
  PacketPtr packet;

  explicit Impl(const std::string& codec_hint)
      : codec_name(codec_hint),
        codec_context(openCodecContext(codec_hint)),
        frame(allocateFrameBuffer()),
        packet(allocatePacketBuffer()) {}
};

const FFmpegDecoder::Impl& FFmpegDecoder::requireImpl() const {
  if (!pimpl_) {
    throw std::logic_error(
        "Operation called on a moved-from FFmpegDecoder, which holds no resources. "
        "Use the move destination, or reassign the source before calling again.");
  }
  return *pimpl_;
}

FFmpegDecoder::FFmpegDecoder(const std::string& codec_hint)
    : pimpl_(std::make_unique<Impl>(codec_hint)) {}

FFmpegDecoder::~FFmpegDecoder() = default;

FFmpegDecoder::FFmpegDecoder(FFmpegDecoder&&) noexcept = default;
FFmpegDecoder& FFmpegDecoder::operator=(FFmpegDecoder&&) noexcept = default;

std::vector<DecodedFrame> FFmpegDecoder::decode(std::span<const uint8_t> compressed_data) {
  requireImpl();
  throw std::logic_error(
      "FFmpegDecoder::decode received " + std::to_string(compressed_data.size()) +
      " byte(s) but packet decoding is not implemented in this build. Returning no frames would "
      "silently drop video data; wire up an IVideoDecoder implementation before decoding.");
}

std::vector<DecodedFrame> FFmpegDecoder::flush() {
  requireImpl();
  return {};
}

CodecInfo FFmpegDecoder::getCodecInfo() const {
  const Impl& impl = requireImpl();
  CodecIdentity identity{impl.codec_name, "", 0, false};
  VideoCodecParameters params{
      impl.codec_context->width, impl.codec_context->height,
      0.0  // Frame rate is derived from the codec time base, which is only known once fed.
  };
  return CodecInfo(std::move(identity), std::move(params));
}

}  // namespace iptv::decoder
