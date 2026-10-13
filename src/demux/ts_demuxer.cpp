extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>
#include <stdexcept>

#include "iptv/demux/ts_demuxer.hpp"

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

// ─── MPEG-TS demuxer ─────────────────────────────────────────────────────────

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

} // anonymous namespace

namespace iptv::demux {



std::vector<DemuxedPacket> TsDemuxer::demux(
    std::span<const uint8_t> ts_data,
    int* video_stream_index,
    int* audio_stream_index) {
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

    // Convert PTS and DTS from stream time_base to microseconds
    AVRational stream_time_base = fmt_ctx->streams[pkt->stream_index]->time_base;
    if (pkt->pts != AV_NOPTS_VALUE) {
      dp.pts = av_rescale_q(pkt->pts, stream_time_base, {1, 1000000});
    } else {
      dp.pts = AV_NOPTS_VALUE;
    }
    if (pkt->dts != AV_NOPTS_VALUE) {
      dp.dts = av_rescale_q(pkt->dts, stream_time_base, {1, 1000000});
    } else {
      dp.dts = AV_NOPTS_VALUE;
    }

    packets.push_back(std::move(dp));
    av_packet_unref(pkt.get());
  }

  return packets;
}

} // namespace iptv::demux