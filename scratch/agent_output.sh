mkdir -p include/iptv/decoder
mkdir -p src/decoder
cat << 'EOF' > include/iptv/decoder/ffmpeg_audio_decoder.hpp
#pragma once

#include <string>
#include <memory>

namespace iptv::decoder {

struct DecodedFrame {
    std::vector<uint8_t> data;
    int pts{0};
    int duration{0};
};

class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;
    virtual DecodedFrame decode(const std::vector<uint8_t>& packet) = 0;
    virtual void flush() = 0;
};

class FFmpegAudioDecoder : public IAudioDecoder {
public:
    explicit FFmpegAudioDecoder(const std::string& codec_hint);
    ~FFmpegAudioDecoder() override = default;

    DecodedFrame decode(const std::vector<uint8_t>& packet) override;
    void flush() override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace iptv::decoder
EOF
cat << 'EOF' > src/decoder/ffmpeg_audio_decoder.cpp
#include "iptv/decoder/ffmpeg_audio_decoder.hpp"
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/averror.h>
#include <vector>
#include <string>

namespace iptv::decoder {

struct FFmpegAudioDecoder::Impl {
    std::unique_ptr<AVCodecContext, void(*)(AVCodecContext*)> codec_ctx{nullptr, avcodec_free_context};
    std::unique_ptr<AVFrame, void(*)(AVFrame*)> frame{nullptr, av_frame_free};
    std::unique_ptr<AVPacket, void(*)(AVPacket*)> packet{nullptr, av_packet_free};
    std::unique_ptr<SwrContext, void(*)(SwrContext*)> swr_ctx{nullptr, swr_free};

    int sample_rate{48000};
    uint64_t channel_layout{AV_CH_LAYOUT_STEREO};
    enum AVSampleFormat sample_fmt{AV_SAMPLE_FMT_S16};

    Impl() = default;
    ~Impl() = default;
};

FFmpegAudioDecoder::FFmpegAudioDecoder(const std::string& codec_hint) : pimpl_(std::make_unique<Impl>()) {
    auto& p = *pimpl_;
    p.sample_rate = 48000;
    p.channel_layout = AV_CH_LAYOUT_STEREO;
    p.sample_fmt = AV_SAMPLE_FMT_S16;

    const char* name = nullptr;
    if (codec_hint == "aac") name = "aac";
    else if (codec_hint == "mp3") name = "mp3";

    if (name) {
        const AVCodec* codec = avcodec_find_decoder_by_name(name);
        if (codec) {
            p.codec_ctx.reset(avcodec_alloc_context3(codec));
            // avcodec_open2(p.codec_ctx.get(), codec, nullptr);
        }
    }

    if (p.codec_ctx) {
        p.swr_ctx.reset(swr_alloc_set_opts(nullptr, p.channel_layout, p.sample_fmt, p.sample_rate,
                            p.codec_ctx->channel_layout, p.codec_ctx->fmt, p.codec_ctx->sample_rate, 0, nullptr));
    }
}

DecodedFrame FFmpegAudioDecoder::decode(const std::vector<uint8_t>& packet_data) {
    DecodedFrame result;
    auto& p = *pimpl_;

    if (!p.packet) {
        p.packet = std::unique_ptr<AVPacket, void(*)(AVPacket*)>(av_packet_alloc(), av_packet_free);
    }
    p.packet->data = const_cast<uint8_t*>(packet_data.data());
    p.packet->size = packet_data.size();

    int ret = avcodec_send_packet(p.codec_ctx.get(), p.packet.get());
    if (ret < 0) return result;

    while (ret >= 0) {
        ret = avcodec_receive_frame(p.codec_ctx.get(), p.frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AV_ERROR_EOF) break;
        if (ret < 0) return result;

        int nb_out = swr_get_out_samples(p.swr_ctx.get(), p.frame->nb_samples);
        int buf_size = av_samples_get_buffer_size(nullptr, 2, nb_out, AV_SAMPLE_FMT_S16);
        if (buf_size < 0) return result;

        std::vector<uint8_t> buf(static_cast<size_t>(buf_size));
        uint8_t* data[1] = { buf.data() };

        swr_convert(p.swr_ctx.get(), data, nb_out,
                    reinterpret_cast<const uint8_t**>(p.frame->data), p.frame->nb_samples);

        result.data = std::move(buf);
        result.pts = p.frame->pts;
        result.duration = p.frame->duration;
        return result;
    }
    return result;
}

void FFmpegAudioDecoder::flush() {
    auto& p = *pimpl_;
    if (p.codec_ctx) avcodec_send_packet(p.codec_ctx.get(), nullptr);
    if (p.swr_ctx) swr_convert(p.swr_ctx.get(), nullptr, 0, nullptr, 0);
}

} // namespace iptv::decoder
EOF
