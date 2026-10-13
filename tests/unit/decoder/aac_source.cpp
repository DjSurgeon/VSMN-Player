#include "aac_source.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace {

constexpr int kFrequencyHz = 440;
constexpr double kAmplitude = 0.5;

struct EncoderContextDeleter {
  void operator()(AVCodecContext* context) const noexcept {
    if (context != nullptr) {
      avcodec_free_context(&context);
    }
  }
};

using EncoderContextPtr = std::unique_ptr<AVCodecContext, EncoderContextDeleter>;

struct FrameDeleter {
  void operator()(AVFrame* frame) const noexcept {
    if (frame != nullptr) {
      av_frame_free(&frame);
    }
  }
};

using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

struct PacketDeleter {
  void operator()(AVPacket* packet) const noexcept {
    if (packet != nullptr) {
      av_packet_free(&packet);
    }
  }
};

using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

EncoderContextPtr openAacEncoder(int sample_rate, int channels) {
  const AVCodec* encoder = avcodec_find_encoder_by_name("aac");
  if (encoder == nullptr) {
    throw std::runtime_error(
        "This FFmpeg build has no AAC encoder, so the audio decoder tests have no "
        "compressed input to decode.");
  }

  EncoderContextPtr context(avcodec_alloc_context3(encoder));
  if (!context) {
    throw std::runtime_error("Failed to allocate an AAC encoder context.");
  }

  context->sample_rate = sample_rate;
  context->ch_layout.nb_channels = channels;
  context->ch_layout.order = AV_CHANNEL_ORDER_NATIVE;
  context->ch_layout.u.mask = (channels == 2) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
  context->sample_fmt = AV_SAMPLE_FMT_FLTP;
  context->bit_rate = 128000;

  if (avcodec_open2(context.get(), encoder, nullptr) < 0) {
    throw std::runtime_error("Failed to open the AAC encoder.");
  }

  return context;
}

FramePtr allocateAudioFrame(int sample_rate, int channels, int nb_samples) {
  FramePtr frame(av_frame_alloc());
  if (!frame) {
    throw std::runtime_error("Failed to allocate an AAC audio frame.");
  }

  frame->format = AV_SAMPLE_FMT_FLTP;
  frame->ch_layout.nb_channels = channels;
  frame->ch_layout.order = AV_CHANNEL_ORDER_NATIVE;
  frame->ch_layout.u.mask = (channels == 2) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
  frame->sample_rate = sample_rate;
  frame->nb_samples = nb_samples;

  if (av_frame_get_buffer(frame.get(), 0) < 0) {
    throw std::runtime_error("Failed to allocate the AAC audio frame buffer.");
  }

  return frame;
}

void fillSineWave(AVFrame* frame, int sample_rate, int channels, int sample_offset) {
  const int nb_samples = frame->nb_samples;
  const double phase_increment = 2.0 * M_PI * kFrequencyHz / sample_rate;

  for (int ch = 0; ch < channels; ++ch) {
    float* samples = reinterpret_cast<float*>(frame->data[ch]);
    for (int i = 0; i < nb_samples; ++i) {
      const int global_sample = sample_offset + i;
      samples[i] = static_cast<float>(kAmplitude * std::sin(phase_increment * global_sample));
    }
  }
}

void add_adts_header(std::vector<uint8_t>& packet, int sample_rate, int channels) {
  int profile = 1; // AAC LC in ADTS is 1 (Profile - 1, where LC is 2)
  int freqIdx = 4; // 44100
  if (sample_rate == 48000) freqIdx = 3;
  int chanCfg = channels;
  int frame_length = static_cast<int>(packet.size()) + 7;

  std::vector<uint8_t> adts(7);
  adts[0] = 0xFF;
  adts[1] = 0xF1;
  adts[2] = static_cast<uint8_t>(((profile & 3) << 6) | ((freqIdx & 0x0F) << 2) | ((chanCfg >> 2) & 1));
  adts[3] = static_cast<uint8_t>(((chanCfg & 3) << 6) | ((frame_length >> 11) & 3));
  adts[4] = static_cast<uint8_t>((frame_length >> 3) & 0xFF);
  adts[5] = static_cast<uint8_t>(((frame_length & 7) << 5) | 0x1F);
  adts[6] = 0xFC;

  packet.insert(packet.begin(), adts.begin(), adts.end());
}

void drainPackets(AVCodecContext* context, AVPacket* packet,
                  std::vector<std::vector<uint8_t>>& packets) {
  while (avcodec_receive_packet(context, packet) == 0) {
    std::vector<uint8_t> pkt_data(packet->data, packet->data + packet->size);
    add_adts_header(pkt_data, context->sample_rate, context->ch_layout.nb_channels);
    packets.push_back(std::move(pkt_data));
    av_packet_unref(packet);
  }
}

void requireEncoderAccepted(int status, const char* stage) {
  if (status < 0) {
    throw std::runtime_error(std::string("AAC encoder ") + stage + " failed with status " +
                             std::to_string(status) + ".");
  }
}

}  // namespace

namespace iptv::decoder::testing {

std::vector<std::vector<uint8_t>> encodeAacSineWave(int sample_rate,
                                                      int channels,
                                                      int duration_ms) {
  const EncoderContextPtr encoder = openAacEncoder(sample_rate, channels);

  const int frame_size = encoder->frame_size;
  if (frame_size <= 0) {
    throw std::runtime_error("AAC encoder reported an invalid frame size.");
  }

  const int total_samples = (sample_rate * duration_ms) / 1000;
  const PacketPtr packet(av_packet_alloc());
  if (!packet) {
    throw std::runtime_error("Failed to allocate an AAC packet.");
  }

  std::vector<std::vector<uint8_t>> packets;
  int sample_offset = 0;

  while (sample_offset < total_samples) {
    const int nb_samples = std::min(frame_size, total_samples - sample_offset);
    FramePtr frame = allocateAudioFrame(sample_rate, channels, nb_samples);
    fillSineWave(frame.get(), sample_rate, channels, sample_offset);

    frame->pts = sample_offset;
    requireEncoderAccepted(avcodec_send_frame(encoder.get(), frame.get()), "frame submission");
    drainPackets(encoder.get(), packet.get(), packets);

    sample_offset += nb_samples;
  }

  requireEncoderAccepted(avcodec_send_frame(encoder.get(), nullptr), "flush");
  drainPackets(encoder.get(), packet.get(), packets);

  return packets;
}

}  // namespace iptv::decoder::testing
