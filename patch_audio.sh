sed -i 's/AVChannelLayout out_ch_layout;/uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;/g' src/decoder/ffmpeg_audio_decoder.cpp
sed -i 's/av_channel_layout_default(&out_ch_layout, 2);/\/\/ No setup needed/g' src/decoder/ffmpeg_audio_decoder.cpp
sed -i 's/av_channel_layout_uninit(&out_ch_layout);/\/\/ No uninit needed/g' src/decoder/ffmpeg_audio_decoder.cpp
sed -i 's/swr_alloc_set_opts2(&raw_swr, &out_ch_layout, AV_SAMPLE_FMT_S16, 48000, \&codec_ctx->ch_layout, codec_ctx->sample_fmt,/swr_alloc_set_opts(\&raw_swr, out_ch_layout, AV_SAMPLE_FMT_S16, 48000, codec_ctx->channel_layout ? codec_ctx->channel_layout : av_get_default_channel_layout(codec_ctx->channels), codec_ctx->sample_fmt,/g' src/decoder/ffmpeg_audio_decoder.cpp
sed -i 's/impl.codec_ctx->ch_layout.nb_channels/impl.codec_ctx->channels/g' src/decoder/ffmpeg_audio_decoder.cpp
