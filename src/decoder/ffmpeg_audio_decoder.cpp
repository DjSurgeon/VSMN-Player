#include "iptv/decoder/ffmpeg_audio_decoder.hpp"

#include <stdexcept>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include "internal/ffmpeg_buffers.hpp"
#include "iptv/decoder/decoder_error.hpp"

namespace iptv::decoder {

namespace {

struct SwrContextDeleter {
  void operator()(SwrContext* ctx) const noexcept { swr_free(&ctx); }
};

using SwrCtxPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

}  // namespace

struct FFmpegAudioDecoder::Impl {
  internal::CodecContextPtr codec_ctx;
  SwrCtxPtr swr_ctx;
  internal::PacketPtr packet;
  internal::FramePtr frame;
  AVChannelLayout out_ch_layout;

  explicit Impl(const std::string& codec_hint) {
    codec_ctx = internal::openCodecContext(codec_hint);
    packet = internal::allocatePacketBuffer();
    frame = internal::allocateFrameBuffer();
    av_channel_layout_default(&out_ch_layout, 2);
  }

  ~Impl() { av_channel_layout_uninit(&out_ch_layout); }

  void initSwrContext() {
    if (swr_ctx)
      return;

    SwrContext* raw_swr = nullptr;
    int ret = swr_alloc_set_opts2(&raw_swr, &out_ch_layout, AV_SAMPLE_FMT_S16, 48000,
                                  &codec_ctx->ch_layout, codec_ctx->sample_fmt,
                                  codec_ctx->sample_rate, 0, nullptr);
    if (ret < 0 || !raw_swr) {
      throw DecoderException(DecoderError::AllocationFailed, "Failed to allocate SwrContext");
    }
    swr_ctx.reset(raw_swr);
    if (swr_init(swr_ctx.get()) < 0) {
      swr_ctx.reset();
      throw DecoderException(DecoderError::UnknownError, "Failed to initialize SwrContext");
    }
  }

  std::vector<DecodedFrame> drainFrames() {
    std::vector<DecodedFrame> output;
    while (true) {
      int ret = avcodec_receive_frame(codec_ctx.get(), frame.get());
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      }
      if (ret < 0) {
        throw DecoderException(DecoderError::UnknownError, "Error receiving audio frame");
      }

      initSwrContext();

      int out_samples = swr_get_out_samples(swr_ctx.get(), frame->nb_samples);
      if (out_samples < 0) {
        throw DecoderException(DecoderError::AllocationFailed, "Failed to calculate out samples");
      }

      int data_size = av_samples_get_buffer_size(nullptr, 2, out_samples, AV_SAMPLE_FMT_S16, 1);
      if (data_size < 0) {
        throw DecoderException(DecoderError::AllocationFailed, "Failed to calculate buffer size");
      }

      std::vector<uint8_t> out_buf(data_size);
      uint8_t* out_data[1] = {out_buf.data()};

      int resampled_samples =
          swr_convert(swr_ctx.get(), out_data, out_samples,
                      const_cast<const uint8_t**>(frame->data), frame->nb_samples);
      if (resampled_samples < 0) {
        throw DecoderException(DecoderError::CorruptInput, "Error while resampling audio");
      }

      int actual_size =
          av_samples_get_buffer_size(nullptr, 2, resampled_samples, AV_SAMPLE_FMT_S16, 1);
      out_buf.resize(actual_size);

      FrameTiming timing{frame->pts, 0};
      AudioFrameGeometry geom{48000, 2, AudioFormat::PCM_S16_48KHZ};
      output.push_back(DecodedFrame(timing, geom, std::move(out_buf)));
    }
    return output;
  }
};

FFmpegAudioDecoder::FFmpegAudioDecoder(const std::string& codec_hint) {
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

FFmpegAudioDecoder::~FFmpegAudioDecoder() = default;

FFmpegAudioDecoder::FFmpegAudioDecoder(FFmpegAudioDecoder&&) noexcept = default;
FFmpegAudioDecoder& FFmpegAudioDecoder::operator=(FFmpegAudioDecoder&&) noexcept = default;

const FFmpegAudioDecoder::Impl& FFmpegAudioDecoder::requireImpl() const {
  if (!pimpl_)
    throw std::logic_error("Moved-from state");
  return *pimpl_;
}

FFmpegAudioDecoder::Impl& FFmpegAudioDecoder::requireMutableImpl() {
  if (!pimpl_)
    throw std::logic_error("Moved-from state");
  return *pimpl_;
}

std::vector<DecodedFrame> FFmpegAudioDecoder::decode(std::span<const uint8_t> compressed_data) {
  auto& impl = requireMutableImpl();

  // Set up packet
  impl.packet->data = const_cast<uint8_t*>(compressed_data.data());
  impl.packet->size = static_cast<int>(compressed_data.size());

  int ret = avcodec_send_packet(impl.codec_ctx.get(), impl.packet.get());
  if (ret < 0 && ret != AVERROR_EOF) {
    throw DecoderException(DecoderError::UnknownError, "Error sending packet to decoder");
  }

  return impl.drainFrames();
}

std::vector<DecodedFrame> FFmpegAudioDecoder::flush() {
  auto& impl = requireMutableImpl();

  avcodec_send_packet(impl.codec_ctx.get(), nullptr);
  auto frames = impl.drainFrames();

  // Drain swresample
  if (impl.swr_ctx) {
    int out_samples = swr_get_out_samples(impl.swr_ctx.get(), 0);
    if (out_samples > 0) {
      int data_size = av_samples_get_buffer_size(nullptr, 2, out_samples, AV_SAMPLE_FMT_S16, 1);
      std::vector<uint8_t> out_buf(data_size);
      uint8_t* out_data[1] = {out_buf.data()};

      int resampled_samples = swr_convert(impl.swr_ctx.get(), out_data, out_samples, nullptr, 0);
      if (resampled_samples > 0) {
        int actual_size =
            av_samples_get_buffer_size(nullptr, 2, resampled_samples, AV_SAMPLE_FMT_S16, 1);
        out_buf.resize(actual_size);

        FrameTiming timing{AV_NOPTS_VALUE, 0};
        AudioFrameGeometry geom{48000, 2, AudioFormat::PCM_S16_48KHZ};
        frames.push_back(DecodedFrame(timing, geom, std::move(out_buf)));
      }
    }
  }

  return frames;
}

CodecInfo FFmpegAudioDecoder::getCodecInfo() const {
  auto& impl = requireImpl();
  CodecIdentity id{impl.codec_ctx->codec->name, "", impl.codec_ctx->bit_rate, false};
  AudioCodecParameters params{impl.codec_ctx->sample_rate, impl.codec_ctx->ch_layout.nb_channels};
  return CodecInfo(std::move(id), params);
}

}  // namespace iptv::decoder
