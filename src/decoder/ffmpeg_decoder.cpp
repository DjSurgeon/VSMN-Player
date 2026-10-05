#include "iptv/decoder/ffmpeg_decoder.hpp"

#include <stdexcept>

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

}  // namespace

struct FFmpegDecoder::Impl {
  std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codec_context;
  std::unique_ptr<AVFrame, AVFrameDeleter> frame;
  std::unique_ptr<AVPacket, AVPacketDeleter> packet;
  std::string codec_name;

  explicit Impl(const std::string& codec_hint) : codec_name(codec_hint) {
    const AVCodec* codec = avcodec_find_decoder_by_name(codec_hint.c_str());
    if (!codec) {
      throw std::runtime_error("FFmpeg codec not found: " + codec_hint);
    }

    codec_context.reset(avcodec_alloc_context3(codec));
    if (!codec_context) {
      throw std::runtime_error("Failed to allocate AVCodecContext");
    }

    if (avcodec_open2(codec_context.get(), codec, nullptr) < 0) {
      throw std::runtime_error("Failed to open FFmpeg codec");
    }

    frame.reset(av_frame_alloc());
    packet.reset(av_packet_alloc());

    if (!frame || !packet) {
      throw std::runtime_error("Failed to allocate FFmpeg frame or packet");
    }
  }
};

FFmpegDecoder::FFmpegDecoder(const std::string& codec_hint) : pimpl_(std::make_unique<Impl>(codec_hint)) {}

FFmpegDecoder::~FFmpegDecoder() = default;

FFmpegDecoder::FFmpegDecoder(FFmpegDecoder&&) noexcept = default;
FFmpegDecoder& FFmpegDecoder::operator=(FFmpegDecoder&&) noexcept = default;

std::vector<DecodedFrame> FFmpegDecoder::decode(std::span<const uint8_t> compressed_data) {
  // Decoding logic will be implemented in the next phase.
  // This phase focuses purely on RAII memory safety and Pimpl isolation.
  (void)compressed_data;
  return {};
}

std::vector<DecodedFrame> FFmpegDecoder::flush() {
  return {};
}

CodecInfo FFmpegDecoder::getCodecInfo() const {
  CodecIdentity identity{pimpl_->codec_name, "", 0, false};
  VideoCodecParameters params{
      pimpl_->codec_context->width,
      pimpl_->codec_context->height,
      0.0  // FPS mapping requires time_base logic
  };
  return CodecInfo(std::move(identity), std::move(params));
}

}  // namespace iptv::decoder
