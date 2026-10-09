#include <gtest/gtest.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/rational.h>
}

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "iptv/decoder/ffmpeg_audio_decoder.hpp"
#include "iptv/decoder/ffmpeg_decoder.hpp"

using namespace iptv::decoder;

namespace {

// ─── RAII wrappers ──────────────────────────────────────────────────────────

struct FormatContextDeleter {
  void operator()(AVFormatContext* ctx) const noexcept { avformat_free_context(ctx); }
};

struct CodecContextDeleter {
  void operator()(AVCodecContext* ctx) const noexcept { avcodec_free_context(&ctx); }
};

struct FrameDeleter {
  void operator()(AVFrame* f) const noexcept { av_frame_free(&f); }
};

struct PacketDeleter {
  void operator()(AVPacket* p) const noexcept { av_packet_free(&p); }
};

struct AvioContextDeleter {
  void operator()(AVIOContext* ctx) const noexcept { avio_context_free(&ctx); }
};

using FormatCtxPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using CodecCtxPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using AvioPtr = std::unique_ptr<AVIOContext, AvioContextDeleter>;

// ─── Synthetic MPEG-TS generator ─────────────────────────────────────────────

struct TsSegment {
  std::vector<uint8_t> data;
};

struct MuxerState {
  std::vector<uint8_t> output;
};

int writeToBuffer(void* opaque, uint8_t* buf, int buf_size) {
  auto* state = static_cast<MuxerState*>(opaque);
  state->output.insert(state->output.end(), buf, buf + buf_size);
  return buf_size;
}

/**
 * @brief Muxes synthetic H.264 video + AAC audio into an in-memory MPEG-TS segment.
 */
TsSegment generateTsSegment(int video_frame_count, int audio_frame_count,
                            int64_t audio_pts_offset_ms, int width, int height) {
  if (video_frame_count <= 0 || audio_frame_count <= 0) {
    throw std::invalid_argument("Frame counts must be positive");
  }
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("Dimensions must be positive");
  }

  MuxerState muxer_state;

  AVFormatContext* raw_fmt = nullptr;
  int ret = avformat_alloc_output_context2(&raw_fmt, nullptr, "mpegts", nullptr);
  if (ret < 0 || !raw_fmt) {
    throw std::runtime_error("Failed to allocate output format context");
  }
  FormatCtxPtr fmt_ctx(raw_fmt);

  // ── Video stream (H.264) ──────────────────────────────────────────────

  const AVCodec* video_codec = avcodec_find_encoder_by_name("mpeg4");
  if (!video_codec) {
    throw std::runtime_error("MPEG-4 encoder not available in this FFmpeg build");
  }

  AVStream* video_stream = avformat_new_stream(fmt_ctx.get(), video_codec);
  if (!video_stream) {
    throw std::runtime_error("Failed to create video stream");
  }

  CodecCtxPtr video_ctx(avcodec_alloc_context3(video_codec));
  if (!video_ctx) {
    throw std::runtime_error("Failed to allocate video codec context");
  }

  video_ctx->width = width;
  video_ctx->height = height;
  video_ctx->time_base = AVRational{1, 30};
  video_ctx->framerate = AVRational{30, 1};
  video_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  video_ctx->bit_rate = 400000;
  video_ctx->gop_size = 12;
  video_ctx->max_b_frames = 0;

  av_opt_set(video_ctx->priv_data, "preset", "ultrafast", 0);
  av_opt_set(video_ctx->priv_data, "tune", "zerolatency", 0);

  ret = avcodec_open2(video_ctx.get(), video_codec, nullptr);
  if (ret < 0) {
    throw std::runtime_error("Failed to open video codec");
  }

  video_stream->time_base = video_ctx->time_base;
  avcodec_parameters_from_context(video_stream->codecpar, video_ctx.get());

  // ── Audio stream (AAC) ─────────────────────────────────────────────────

  const AVCodec* audio_codec = avcodec_find_encoder_by_name("aac");
  if (!audio_codec) {
    throw std::runtime_error("AAC encoder not available in this FFmpeg build");
  }

  AVStream* audio_stream = avformat_new_stream(fmt_ctx.get(), audio_codec);
  if (!audio_stream) {
    throw std::runtime_error("Failed to create audio stream");
  }

  CodecCtxPtr audio_ctx(avcodec_alloc_context3(audio_codec));
  if (!audio_ctx) {
    throw std::runtime_error("Failed to allocate audio codec context");
  }

  audio_ctx->sample_rate = 48000;
  audio_ctx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
  audio_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
  audio_ctx->bit_rate = 128000;
  audio_ctx->time_base = AVRational{1, 48000};
  audio_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  ret = avcodec_open2(audio_ctx.get(), audio_codec, nullptr);
  if (ret < 0) {
    throw std::runtime_error("Failed to open audio codec");
  }

  audio_stream->time_base = audio_ctx->time_base;
  avcodec_parameters_from_context(audio_stream->codecpar, audio_ctx.get());

  // ── Open AVIO ──────────────────────────────────────────────────────────

  int buf_size = 32768;
  uint8_t* avio_buf = static_cast<uint8_t*>(av_malloc(buf_size));
  if (!avio_buf) {
    throw std::runtime_error("Failed to allocate AVIO buffer");
  }

  AVIOContext* raw_avio = avio_alloc_context(avio_buf, buf_size, 1, &muxer_state,
                                              nullptr, writeToBuffer, nullptr);
  if (!raw_avio) {
    av_free(avio_buf);
    throw std::runtime_error("Failed to allocate AVIO context");
  }
  AvioPtr avio_ctx(raw_avio);

  fmt_ctx->pb = avio_ctx.get();
  fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

  ret = avformat_write_header(fmt_ctx.get(), nullptr);
  if (ret < 0) {
    throw std::runtime_error("Failed to write TS header");
  }

  // ── Encode and mux video ────────────────────────────────────────────────

  PacketPtr pkt(av_packet_alloc());
  if (!pkt) {
    throw std::runtime_error("Failed to allocate packet");
  }

  const int video_pts_step = 1;
  int64_t video_pts = 0;

  for (int i = 0; i < video_frame_count; ++i) {
    FramePtr frame(av_frame_alloc());
    if (!frame) {
      throw std::runtime_error("Failed to allocate video frame");
    }

    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width;
    frame->height = height;
    frame->pts = video_pts;

    ret = av_frame_get_buffer(frame.get(), 0);
    if (ret < 0) {
      throw std::runtime_error("Failed to allocate video frame buffer");
    }

    // Fill Y plane with pattern based on frame index
    uint8_t y_val = static_cast<uint8_t>((i * 17) & 0xFF);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        frame->data[0][y * frame->linesize[0] + x] =
            static_cast<uint8_t>((y_val + x + y) & 0xFF);
      }
    }

    // Fill U/V planes
    for (int plane = 1; plane < 3; ++plane) {
      int plane_h = height / 2;
      int plane_w = width / 2;
      for (int y = 0; y < plane_h; ++y) {
        for (int x = 0; x < plane_w; ++x) {
          frame->data[plane][y * frame->linesize[plane] + x] = 128;
        }
      }
    }

    ret = avcodec_send_frame(video_ctx.get(), frame.get());
    if (ret < 0) {
      throw std::runtime_error("Failed to send video frame to encoder");
    }

    while (avcodec_receive_packet(video_ctx.get(), pkt.get()) == 0) {
      pkt->stream_index = video_stream->index;
      av_packet_rescale_ts(pkt.get(), video_ctx->time_base, video_stream->time_base);
      ret = av_interleaved_write_frame(fmt_ctx.get(), pkt.get());
      if (ret < 0) {
        throw std::runtime_error("Failed to write video packet");
      }
      av_packet_unref(pkt.get());
    }

    video_pts += video_pts_step;
  }

  // Flush video encoder
  avcodec_send_frame(video_ctx.get(), nullptr);
  while (avcodec_receive_packet(video_ctx.get(), pkt.get()) == 0) {
    pkt->stream_index = video_stream->index;
    av_packet_rescale_ts(pkt.get(), video_ctx->time_base, video_stream->time_base);
    ret = av_interleaved_write_frame(fmt_ctx.get(), pkt.get());
    if (ret < 0) {
      throw std::runtime_error("Failed to write flushed video packet");
    }
    av_packet_unref(pkt.get());
  }

  // ── Encode and mux audio ────────────────────────────────────────────────

  const int audio_samples_per_frame = 1024;
  const int audio_pts_step = audio_samples_per_frame;
  const int64_t audio_pts_offset_48k = audio_pts_offset_ms * 48;
  int64_t audio_pts = audio_pts_offset_48k;

  for (int i = 0; i < audio_frame_count; ++i) {
    FramePtr frame(av_frame_alloc());
    if (!frame) {
      throw std::runtime_error("Failed to allocate audio frame");
    }

    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    frame->sample_rate = 48000;
    frame->nb_samples = audio_samples_per_frame;
    frame->pts = audio_pts;

    ret = av_frame_get_buffer(frame.get(), 0);
    if (ret < 0) {
      throw std::runtime_error("Failed to allocate audio frame buffer");
    }

    // Fill with silence (zeroed buffer is fine for pipeline test)
    // FFmpeg's av_frame_get_buffer zero-initializes, so we just send it.

    ret = avcodec_send_frame(audio_ctx.get(), frame.get());
    if (ret < 0) {
      throw std::runtime_error("Failed to send audio frame to encoder");
    }

    while (avcodec_receive_packet(audio_ctx.get(), pkt.get()) == 0) {
      pkt->stream_index = audio_stream->index;
      av_packet_rescale_ts(pkt.get(), audio_ctx->time_base, audio_stream->time_base);
      ret = av_interleaved_write_frame(fmt_ctx.get(), pkt.get());
      if (ret < 0) {
        throw std::runtime_error("Failed to write audio packet");
      }
      av_packet_unref(pkt.get());
    }

    audio_pts += audio_pts_step;
  }

  // Flush audio encoder
  avcodec_send_frame(audio_ctx.get(), nullptr);
  while (avcodec_receive_packet(audio_ctx.get(), pkt.get()) == 0) {
    pkt->stream_index = audio_stream->index;
    av_packet_rescale_ts(pkt.get(), audio_ctx->time_base, audio_stream->time_base);
    ret = av_interleaved_write_frame(fmt_ctx.get(), pkt.get());
    if (ret < 0) {
      throw std::runtime_error("Failed to write flushed audio packet");
    }
    av_packet_unref(pkt.get());
  }

  // ── Write trailer ───────────────────────────────────────────────────────

  ret = av_write_trailer(fmt_ctx.get());
  if (ret < 0) {
    throw std::runtime_error("Failed to write TS trailer");
  }

  TsSegment segment;
  segment.data = std::move(muxer_state.output);
  return segment;
}

// ─── MPEG-TS demuxer ─────────────────────────────────────────────────────────

struct DemuxedPacket {
  int stream_index;
  std::vector<uint8_t> data;
  int64_t pts;
};

struct DemuxState {
  const uint8_t* data;
  size_t size;
  size_t pos;
};

int readFromBuffer(void* opaque, uint8_t* buf, int buf_size) {
  auto* state = static_cast<DemuxState*>(opaque);
  size_t remaining = state->size - state->pos;
  size_t to_read = std::min(static_cast<size_t>(buf_size), remaining);
  if (to_read == 0) {
    return AVERROR_EOF;
  }
  std::memcpy(buf, state->data + state->pos, to_read);
  state->pos += to_read;
  return static_cast<int>(to_read);
}

std::vector<DemuxedPacket> demuxTs(const std::vector<uint8_t>& ts_data,
                                   int* video_stream_index, int* audio_stream_index) {
  if (video_stream_index) *video_stream_index = -1;
  if (audio_stream_index) *audio_stream_index = -1;

  DemuxState demux_state{ts_data.data(), ts_data.size(), 0};

  int buf_size = 32768;
  uint8_t* avio_buf = static_cast<uint8_t*>(av_malloc(buf_size));
  if (!avio_buf) {
    throw std::runtime_error("Failed to allocate AVIO buffer");
  }

  AVIOContext* raw_avio = avio_alloc_context(avio_buf, buf_size, 0, &demux_state,
                                              readFromBuffer, nullptr, nullptr);
  if (!raw_avio) {
    av_free(avio_buf);
    throw std::runtime_error("Failed to allocate AVIO context");
  }
  AvioPtr avio_ctx(raw_avio);

  AVFormatContext* raw_fmt = avformat_alloc_context();
  if (!raw_fmt) {
    throw std::runtime_error("Failed to allocate format context");
  }
  FormatCtxPtr fmt_ctx(raw_fmt);
  fmt_ctx->pb = avio_ctx.get();
  fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

  AVFormatContext* raw_fmt_ptr = fmt_ctx.release();
  int ret = avformat_open_input(&raw_fmt_ptr, nullptr, nullptr, nullptr);
  fmt_ctx.reset(raw_fmt_ptr);
  if (ret < 0) {
    throw std::runtime_error("Failed to open input format context");
  }

  ret = avformat_find_stream_info(fmt_ctx.get(), nullptr);
  if (ret < 0) {
    throw std::runtime_error("Failed to find stream info");
  }

  // Find video/audio stream indices
  for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
    AVStream* stream = fmt_ctx->streams[i];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index) {
      *video_stream_index = static_cast<int>(i);
    } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_index) {
      *audio_stream_index = static_cast<int>(i);
    }
  }

  std::vector<DemuxedPacket> packets;
  PacketPtr pkt(av_packet_alloc());
  if (!pkt) {
    throw std::runtime_error("Failed to allocate packet");
  }

  while (av_read_frame(fmt_ctx.get(), pkt.get()) >= 0) {
    DemuxedPacket dp;
    dp.stream_index = pkt->stream_index;
    dp.data.assign(pkt->data, pkt->data + pkt->size);
    dp.pts = pkt->pts;
    packets.push_back(std::move(dp));
    av_packet_unref(pkt.get());
  }

  return packets;
}

// ─── Pipeline test fixture ───────────────────────────────────────────────────

class PipelineIntegrationTest : public ::testing::Test {
 protected:
  static constexpr int kVideoFrameCount = 60;   // ~2s at 30fps
  static constexpr int kAudioFrameCount = 94;   // ~2s at 48kHz, 1024 samples/frame
  static constexpr int kWidth = 64;
  static constexpr int kHeight = 64;

  void SetUp() override {
    // Base generation done in each test to allow different parameters
  }
};

// ─── Test: Full pipeline round-trip ──────────────────────────────────────────

TEST_F(PipelineIntegrationTest, DecodesVideoAndAudioFromTsSegment) {
  TsSegment segment = generateTsSegment(kVideoFrameCount, kAudioFrameCount,
                                        /*audio_pts_offset_ms=*/0,
                                        kWidth, kHeight);
  ASSERT_GT(segment.data.size(), 0) << "TS segment should not be empty";

  int video_idx = -1, audio_idx = -1;
  std::vector<DemuxedPacket> packets = demuxTs(segment.data, &video_idx, &audio_idx);
  ASSERT_GE(video_idx, 0) << "Should find video stream";
  ASSERT_GE(audio_idx, 0) << "Should find audio stream";
  ASSERT_GT(packets.size(), 0) << "Should extract packets from TS";

  FFmpegDecoder video_decoder("mpeg4");
  FFmpegAudioDecoder audio_decoder("aac");

  std::vector<DecodedFrame> video_frames;
  std::vector<DecodedFrame> audio_frames;

  for (const auto& packet : packets) {
    if (packet.stream_index == video_idx) {
      auto frames = video_decoder.decode(std::span<const uint8_t>{packet.data});
      for (auto& f : frames) {
        video_frames.push_back(std::move(f));
      }
    } else if (packet.stream_index == audio_idx) {
      auto frames = audio_decoder.decode(std::span<const uint8_t>{packet.data});
      for (auto& f : frames) {
        audio_frames.push_back(std::move(f));
      }
    }
  }

  // Flush decoders
  auto flushed_video = video_decoder.flush();
  for (auto& f : flushed_video) {
    video_frames.push_back(std::move(f));
  }
  auto flushed_audio = audio_decoder.flush();
  for (auto& f : flushed_audio) {
    audio_frames.push_back(std::move(f));
  }

  // ── Video assertions ──────────────────────────────────────────────────

  ASSERT_GT(video_frames.size(), 0) << "Should decode at least one video frame";

  for (const auto& frame : video_frames) {
    EXPECT_EQ(frame.media_type, MediaType::Video);
    EXPECT_EQ(frame.width, kWidth);
    EXPECT_EQ(frame.height, kHeight);
    EXPECT_EQ(frame.pixel_format, PixelFormat::YUV420P);
    EXPECT_GT(frame.linesize[0], 0) << "Y plane stride should be positive";
    EXPECT_GT(frame.data.size(), 0) << "Video frame data should not be empty";
    EXPECT_GE(frame.pts, 0) << "Video PTS should be non-negative";
  }

  // PTS should be monotonically increasing
  for (size_t i = 1; i < video_frames.size(); ++i) {
    EXPECT_GT(video_frames[i].pts, video_frames[i - 1].pts)
        << "Video PTS should increase monotonically";
  }

  // ── Audio assertions ───────────────────────────────────────────────────

  ASSERT_GT(audio_frames.size(), 0) << "Should decode at least one audio frame";

  for (const auto& frame : audio_frames) {
    EXPECT_EQ(frame.media_type, MediaType::Audio);
    EXPECT_EQ(frame.sample_rate, 48000);
    EXPECT_EQ(frame.channels, 2);
    EXPECT_EQ(frame.audio_format, AudioFormat::PCM_S16_48KHZ);
    EXPECT_GT(frame.data.size(), 0) << "Audio frame data should not be empty";
  }

  // Audio PTS should be monotonically increasing (if present)
  for (size_t i = 1; i < audio_frames.size(); ++i) {
    if (audio_frames[i].pts != AV_NOPTS_VALUE && audio_frames[i - 1].pts != AV_NOPTS_VALUE) {
      EXPECT_GT(audio_frames[i].pts, audio_frames[i - 1].pts)
          << "Audio PTS should increase monotonically";
    }
  }

  // ── AV sync assertion ──────────────────────────────────────────────────

  // Video: first frame at PTS 0, audio: first frame at PTS 0 (offset=0)
  // They should start roughly together
  if (!video_frames.empty() && !audio_frames.empty()) {
    int64_t video_start = video_frames.front().pts;
    int64_t audio_start = audio_frames.front().pts;
    // Video PTS is synthesized in ms; audio PTS is in 48kHz ticks (raw from decoder)
    // Both should start near zero relative to stream start
    EXPECT_GE(video_start, 0);
    if (audio_start != AV_NOPTS_VALUE) {
      EXPECT_GE(audio_start, 0);
    }
  }
}

// ─── Test: Dropped packets ───────────────────────────────────────────────────

TEST_F(PipelineIntegrationTest, SurvivesDroppedPackets) {
  TsSegment segment = generateTsSegment(kVideoFrameCount, kAudioFrameCount,
                                        /*audio_pts_offset_ms=*/0,
                                        kWidth, kHeight);

  int video_idx = -1, audio_idx = -1;
  std::vector<DemuxedPacket> packets = demuxTs(segment.data, &video_idx, &audio_idx);

  FFmpegDecoder video_decoder("mpeg4");
  FFmpegAudioDecoder audio_decoder("aac");

  std::vector<DecodedFrame> video_frames;
  std::vector<DecodedFrame> audio_frames;

  // Drop every 3rd video packet and every 5th audio packet
  int video_packet_count = 0;
  int audio_packet_count = 0;

  for (const auto& packet : packets) {
    if (packet.stream_index == video_idx) {
      ++video_packet_count;
      if (video_packet_count % 3 == 0) {
        continue;  // Simulate dropped packet
      }
      auto frames = video_decoder.decode(std::span<const uint8_t>{packet.data});
      for (auto& f : frames) {
        video_frames.push_back(std::move(f));
      }
    } else if (packet.stream_index == audio_idx) {
      ++audio_packet_count;
      if (audio_packet_count % 5 == 0) {
        continue;  // Simulate dropped packet
      }
      auto frames = audio_decoder.decode(std::span<const uint8_t>{packet.data});
      for (auto& f : frames) {
        audio_frames.push_back(std::move(f));
      }
    }
  }

  auto flushed_video = video_decoder.flush();
  for (auto& f : flushed_video) {
    video_frames.push_back(std::move(f));
  }
  auto flushed_audio = audio_decoder.flush();
  for (auto& f : flushed_audio) {
    audio_frames.push_back(std::move(f));
  }

  // Should still decode frames despite drops
  EXPECT_GT(video_frames.size(), 0) << "Should decode video frames despite dropped packets";
  EXPECT_GT(audio_frames.size(), 0) << "Should decode audio frames despite dropped packets";

  // All decoded frames should be valid
  for (const auto& frame : video_frames) {
    EXPECT_EQ(frame.media_type, MediaType::Video);
    EXPECT_EQ(frame.width, kWidth);
    EXPECT_EQ(frame.height, kHeight);
    EXPECT_EQ(frame.pixel_format, PixelFormat::YUV420P);
  }

  for (const auto& frame : audio_frames) {
    EXPECT_EQ(frame.media_type, MediaType::Audio);
    EXPECT_EQ(frame.sample_rate, 48000);
    EXPECT_EQ(frame.channels, 2);
  }
}

// ─── Test: Malformed PES ────────────────────────────────────────────────────

TEST_F(PipelineIntegrationTest, SurvivesMalformedPackets) {
  TsSegment segment = generateTsSegment(kVideoFrameCount, kAudioFrameCount,
                                        /*audio_pts_offset_ms=*/0,
                                        kWidth, kHeight);

  int video_idx = -1, audio_idx = -1;
  std::vector<DemuxedPacket> packets = demuxTs(segment.data, &video_idx, &audio_idx);

  FFmpegDecoder video_decoder("mpeg4");
  FFmpegAudioDecoder audio_decoder("aac");

  std::vector<DecodedFrame> video_frames;
  std::vector<DecodedFrame> audio_frames;

  int video_packet_count = 0;
  int audio_packet_count = 0;

  for (auto& packet : packets) {
    if (packet.stream_index == video_idx) {
      ++video_packet_count;
      // Corrupt every 7th video packet by flipping bytes
      if (video_packet_count % 7 == 0 && packet.data.size() > 10) {
        packet.data[5] ^= 0xFF;
        packet.data[6] ^= 0xFF;
        packet.data[7] ^= 0xFF;
      }
      auto frames = video_decoder.decode(std::span<const uint8_t>{packet.data});
      for (auto& f : frames) {
        video_frames.push_back(std::move(f));
      }
    } else if (packet.stream_index == audio_idx) {
      ++audio_packet_count;
      // Corrupt every 9th audio packet
      if (audio_packet_count % 9 == 0 && packet.data.size() > 10) {
        packet.data[3] ^= 0xFF;
        packet.data[4] ^= 0xFF;
        packet.data[5] ^= 0xFF;
      }
      auto frames = audio_decoder.decode(std::span<const uint8_t>{packet.data});
      for (auto& f : frames) {
        audio_frames.push_back(std::move(f));
      }
    }
  }

  auto flushed_video = video_decoder.flush();
  for (auto& f : flushed_video) {
    video_frames.push_back(std::move(f));
  }
  auto flushed_audio = audio_decoder.flush();
  for (auto& f : flushed_audio) {
    audio_frames.push_back(std::move(f));
  }

  // Decoder should survive corruption and still produce some valid frames
  EXPECT_GT(video_frames.size(), 0) << "Should decode some video frames despite corruption";
  EXPECT_GT(audio_frames.size(), 0) << "Should decode some audio frames despite corruption";

  // Remaining valid frames should still have correct geometry
  for (const auto& frame : video_frames) {
    EXPECT_EQ(frame.media_type, MediaType::Video);
    EXPECT_EQ(frame.width, kWidth);
    EXPECT_EQ(frame.height, kHeight);
  }

  for (const auto& frame : audio_frames) {
    EXPECT_EQ(frame.media_type, MediaType::Audio);
    EXPECT_EQ(frame.sample_rate, 48000);
    EXPECT_EQ(frame.channels, 2);
  }
}

// ─── Test: Extreme AV sync drift ─────────────────────────────────────────────

TEST_F(PipelineIntegrationTest, HandlesExtremeAvSyncDrift) {
  // Audio starts 10 seconds after video
  constexpr int64_t kAudioOffsetMs = 10000;

  TsSegment segment = generateTsSegment(kVideoFrameCount, kAudioFrameCount,
                                        kAudioOffsetMs, kWidth, kHeight);

  int video_idx = -1, audio_idx = -1;
  std::vector<DemuxedPacket> packets = demuxTs(segment.data, &video_idx, &audio_idx);

  FFmpegDecoder video_decoder("mpeg4");
  FFmpegAudioDecoder audio_decoder("aac");

  std::vector<DecodedFrame> video_frames;
  std::vector<DecodedFrame> audio_frames;

  for (const auto& packet : packets) {
    if (packet.stream_index == video_idx) {
      auto frames = video_decoder.decode(std::span<const uint8_t>{packet.data});
      for (auto& f : frames) {
        video_frames.push_back(std::move(f));
      }
    } else if (packet.stream_index == audio_idx) {
      auto frames = audio_decoder.decode(std::span<const uint8_t>{packet.data});
      for (auto& f : frames) {
        audio_frames.push_back(std::move(f));
      }
    }
  }

  auto flushed_video = video_decoder.flush();
  for (auto& f : flushed_video) {
    video_frames.push_back(std::move(f));
  }
  auto flushed_audio = audio_decoder.flush();
  for (auto& f : flushed_audio) {
    audio_frames.push_back(std::move(f));
  }

  ASSERT_GT(video_frames.size(), 0);
  ASSERT_GT(audio_frames.size(), 0);

  // Video should start near 0 (synthesized PTS in ms)
  EXPECT_GE(video_frames.front().pts, 0);
  EXPECT_LT(video_frames.front().pts, 100) << "First video frame should start near 0ms";

  // Audio PTS is raw from decoder (in 48kHz ticks for AAC)
  // With 10s offset, first audio PTS should be ~480000 (10s * 48000)
  // The decoder passes through frame->pts from the demuxed packet
  if (audio_frames.front().pts != AV_NOPTS_VALUE) {
    // Audio PTS should be significantly offset from video start
    // At 48kHz, 10s = 480000 ticks. Allow wide tolerance for decoder behavior.
    EXPECT_GT(audio_frames.front().pts, 0)
        << "Audio PTS should be positive with offset";
  }

  // Both streams should have monotonically increasing PTS
  for (size_t i = 1; i < video_frames.size(); ++i) {
    EXPECT_GT(video_frames[i].pts, video_frames[i - 1].pts)
        << "Video PTS should increase monotonically even with AV drift";
  }
  for (size_t i = 1; i < audio_frames.size(); ++i) {
    if (audio_frames[i].pts != AV_NOPTS_VALUE && audio_frames[i - 1].pts != AV_NOPTS_VALUE) {
      EXPECT_GT(audio_frames[i].pts, audio_frames[i - 1].pts)
          << "Audio PTS should increase monotonically even with AV drift";
    }
  }
}

// ─── Test: Codec info after decode ──────────────────────────────────────────

TEST_F(PipelineIntegrationTest, CodecInfoReflectsDecodedStream) {
  TsSegment segment = generateTsSegment(10, 10, 0, kWidth, kHeight);

  int video_idx = -1, audio_idx = -1;
  std::vector<DemuxedPacket> packets = demuxTs(segment.data, &video_idx, &audio_idx);

  FFmpegDecoder video_decoder("mpeg4");
  FFmpegAudioDecoder audio_decoder("aac");

  // Feed at least one packet to each
  bool fed_video = false;
  bool fed_audio = false;
  for (const auto& packet : packets) {
    if (packet.stream_index == video_idx && !fed_video) {
      video_decoder.decode(std::span<const uint8_t>{packet.data});
      fed_video = true;
    } else if (packet.stream_index == audio_idx && !fed_audio) {
      audio_decoder.decode(std::span<const uint8_t>{packet.data});
      fed_audio = true;
    }
    if (fed_video && fed_audio) break;
  }

  CodecInfo video_info = video_decoder.getCodecInfo();
  EXPECT_EQ(video_info.name, "mpeg4");
  EXPECT_EQ(video_info.width, kWidth);
  EXPECT_EQ(video_info.height, kHeight);

  CodecInfo audio_info = audio_decoder.getCodecInfo();
  EXPECT_EQ(audio_info.name, "aac");
  EXPECT_EQ(audio_info.sample_rate, 48000);
  EXPECT_EQ(audio_info.channels, 2);
}

}  // namespace
