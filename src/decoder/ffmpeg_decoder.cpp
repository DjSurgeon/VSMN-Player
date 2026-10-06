#include "iptv/decoder/ffmpeg_decoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "internal/ffmpeg_buffers.hpp"
#include "internal/ffmpeg_frame_extractor.hpp"
#include "iptv/decoder/decoder_error.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}

namespace iptv::decoder {

namespace {

using internal::allocateFrameBuffer;
using internal::allocatePacketBuffer;
using internal::CodecContextPtr;
using internal::FramePtr;
using internal::kFrameTimeBase;
using internal::openCodecContext;
using internal::PacketPtr;

// Smallest step a synthesized timestamp advances by, so frames stay ordered in time order even
// when the codec reports no frame rate to derive a real duration from.
constexpr int64_t kMinimumFrameStepMs = 1;

/**
 * @brief Names an FFmpeg status code, which is otherwise an opaque integer.
 */
std::string describeStatus(int status) {
  char message[AV_ERROR_MAX_STRING_SIZE] = {};
  av_strerror(status, message, sizeof(message));
  return std::string(message);
}

/**
 * @brief Reports whether a status means the input packet was unusable.
 *
 * A corrupt packet is the stream's fault, not this decoder's: the codec rejects it and stays
 * usable for the next one, so the packet is dropped instead of failing the whole call.
 */
[[nodiscard]] bool isCorruptInput(int status) noexcept { return status == AVERROR_INVALIDDATA; }

}  // namespace

struct FFmpegDecoder::Impl {
  std::string codec_name;
  CodecContextPtr codec_context;
  FramePtr frame;
  PacketPtr packet;
  int64_t next_pts_ms{0};
  bool end_of_stream{false};

  explicit Impl(const std::string& codec_hint)
      : codec_name(codec_hint),
        codec_context(openCodecContext(codec_hint)),
        frame(allocateFrameBuffer()),
        packet(allocatePacketBuffer()) {}

  // Feeds one packet to the codec and appends every frame it completes on it.
  void decodePacket(std::span<const uint8_t> compressed_data, std::vector<DecodedFrame>& frames);

  // Signals end of stream, then appends every frame the codec was still holding.
  void drainRemainingFrames(std::vector<DecodedFrame>& frames);

  // Appends every frame the codec currently has ready, in output order.
  void releaseReadyFrames(std::vector<DecodedFrame>& frames);

  // Packs the frame the codec just produced, stamped with the next synthesized time.
  [[nodiscard]] DecodedFrame packReadyFrame();

  // How long a frame stays on screen in milliseconds; 0 when the codec reports no frame rate.
  [[nodiscard]] int64_t frameDurationMs() const noexcept;
};

/**
 * @brief Reports whether the codec took the packet.
 * @param status Status returned when the packet was sent.
 * @param packet_size Size of the packet, quoted in the failure message.
 * @return True when the packet was taken, false when it was corrupt and safely dropped.
 * @throws DecoderException when FFmpeg failed for a reason other than unusable input.
 */
[[nodiscard]] bool requirePacketAccepted(int status, std::size_t packet_size) {
  if (status >= 0) {
    return true;
  }
  if (isCorruptInput(status)) {
    return false;
  }
  throw DecoderException(DecoderError::UnknownError,
                         "FFmpeg rejected a packet of " + std::to_string(packet_size) +
                             " byte(s): " + describeStatus(status) + ".");
}

int64_t FFmpegDecoder::Impl::frameDurationMs() const noexcept {
  const AVRational rate = codec_context->framerate;
  if (rate.num <= 0 || rate.den <= 0) {
    return 0;
  }
  // The codec reports a rate in frames per second; a duration is the inverse of that rate,
  // expressed in the time base the decoder was pinned to.
  return av_rescale_q(1, av_inv_q(rate), kFrameTimeBase);
}

DecodedFrame FFmpegDecoder::Impl::packReadyFrame() {
  const int64_t duration = frameDurationMs();
  DecodedFrame packed = internal::extractDecodedFrame(*frame, next_pts_ms, duration);
  next_pts_ms += std::max(duration, kMinimumFrameStepMs);
  return packed;
}

void FFmpegDecoder::Impl::releaseReadyFrames(std::vector<DecodedFrame>& frames) {
  // One packet does not yield one frame: a codec may hold several pictures back for reordering
  // and may release a different number per call, so frames are collected until it says it needs
  // more input or has nothing left.
  while (true) {
    const int status = avcodec_receive_frame(codec_context.get(), frame.get());
    if (status == 0) {
      frames.push_back(packReadyFrame());
      continue;
    }
    if (isCorruptInput(status)) {
      return;  // This frame is unusable; later packets can still decode.
    }
    if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
      return;
    }
    throw DecoderException(DecoderError::UnknownError,
                           "FFmpeg failed to produce a frame: " + describeStatus(status) + ".");
  }
}

void FFmpegDecoder::Impl::decodePacket(std::span<const uint8_t> compressed_data,
                                       std::vector<DecodedFrame>& frames) {
  if (compressed_data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw DecoderException(DecoderError::UnknownError,
                           "Packet of " + std::to_string(compressed_data.size()) +
                               " byte(s) is larger than the biggest packet FFmpeg can address.");
  }

  // The bytes are copied into a buffer FFmpeg owns rather than pointed at: sending a packet hands
  // the codec a reference to it, and a reference can outlive this call when the codec is holding
  // pictures back for reordering. Reading caller memory then would be a use-after-free.
  av_packet_unref(packet.get());
  const int allocated = av_new_packet(packet.get(), static_cast<int>(compressed_data.size()));
  if (allocated < 0) {
    throw DecoderException(DecoderError::AllocationFailed,
                           "Failed to allocate an FFmpeg packet for " +
                               std::to_string(compressed_data.size()) + " byte(s) of input.");
  }
  std::memcpy(packet->data, compressed_data.data(), compressed_data.size());

  const int status = avcodec_send_packet(codec_context.get(), packet.get());
  av_packet_unref(packet.get());  // The codec keeps its own reference to whatever it still needs.

  if (requirePacketAccepted(status, compressed_data.size())) {
    releaseReadyFrames(frames);
  }
}

void FFmpegDecoder::Impl::drainRemainingFrames(std::vector<DecodedFrame>& frames) {
  if (end_of_stream) {
    return;  // Already drained; a second flush has nothing left to release.
  }
  const int status = avcodec_send_packet(codec_context.get(), nullptr);
  end_of_stream = true;

  if (status < 0 && status != AVERROR_EOF) {
    throw DecoderException(DecoderError::UnknownError, "FFmpeg refused the end of stream signal: " +
                                                           describeStatus(status) + ".");
  }
  releaseReadyFrames(frames);
}

/**
 * @brief Reports the one failure a moved-from decoder can produce.
 * @throws std::logic_error always; a moved-from decoder holds no resources to reach into.
 */
[[noreturn]] void throwMovedFromDecoder() {
  throw std::logic_error(
      "Operation called on a moved-from FFmpegDecoder, which holds no resources. "
      "Use the move destination, or reassign the source before calling again.");
}

const FFmpegDecoder::Impl& FFmpegDecoder::requireImpl() const {
  if (!pimpl_) {
    throwMovedFromDecoder();
  }
  return *pimpl_;
}

FFmpegDecoder::Impl& FFmpegDecoder::requireImpl() {
  if (!pimpl_) {
    throwMovedFromDecoder();
  }
  return *pimpl_;
}

FFmpegDecoder::FFmpegDecoder(const std::string& codec_hint) {
  if (codec_hint.empty()) {
    throw DecoderException(DecoderError::InvalidCodec,
                           "Codec hint must be non-empty; automatic codec detection is "
                           "disabled and a decoder name is required.");
  }
  try {
    pimpl_ = std::make_unique<Impl>(codec_hint);
  } catch (const std::runtime_error& e) {
    throw DecoderException(DecoderError::InvalidCodec, e.what());
  }
}

FFmpegDecoder::~FFmpegDecoder() = default;

FFmpegDecoder::FFmpegDecoder(FFmpegDecoder&&) noexcept = default;
FFmpegDecoder& FFmpegDecoder::operator=(FFmpegDecoder&&) noexcept = default;

std::vector<DecodedFrame> FFmpegDecoder::decode(std::span<const uint8_t> compressed_data) {
  Impl& impl = requireImpl();
  std::vector<DecodedFrame> frames;
  if (compressed_data.empty()) {
    return frames;  // No bytes to hand the codec: an empty packet is not a picture.
  }
  if (impl.end_of_stream) {
    throw std::logic_error(
        "FFmpegDecoder::decode was called after flush(), which closed the codec to input. Any "
        "further packet would be dropped without notice, so this is reported instead.");
  }
  impl.decodePacket(compressed_data, frames);
  return frames;
}

std::vector<DecodedFrame> FFmpegDecoder::flush() {
  Impl& impl = requireImpl();
  std::vector<DecodedFrame> frames;
  impl.drainRemainingFrames(frames);
  return frames;
}

CodecInfo FFmpegDecoder::getCodecInfo() const {
  const Impl& impl = requireImpl();
  CodecIdentity identity{impl.codec_name, "", 0, false};
  const AVRational rate = impl.codec_context->framerate;
  // The codec reports its frame rate from the stream it decoded, so it stays unknown until fed.
  const double fps = (rate.num > 0 && rate.den > 0) ? av_q2d(rate) : 0.0;
  VideoCodecParameters params{
      impl.codec_context->width,
      impl.codec_context->height,
      fps,
  };
  return CodecInfo(std::move(identity), std::move(params));
}

}  // namespace iptv::decoder
