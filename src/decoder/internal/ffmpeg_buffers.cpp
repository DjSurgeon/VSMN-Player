#include "internal/ffmpeg_buffers.hpp"
#include <iptv/decoder/decoder_error.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace iptv::decoder::internal {

CodecContextPtr openCodecContext(const std::string& codec_hint) {
  const AVCodec* codec = avcodec_find_decoder_by_name(codec_hint.c_str());
  if (codec == nullptr) {
    throw DecoderException(DecoderError::CodecNotFound, "FFmpeg codec not found: '" + codec_hint +
                             "'. Supply a decoder name supported by this FFmpeg build.");
  }

  CodecContextPtr context(avcodec_alloc_context3(codec));
  if (!context) {
    throw DecoderException(DecoderError::AllocationFailed, "Failed to allocate AVCodecContext for codec '" + codec_hint +
                             "'. The system is out of memory.");
  }

  // Decoding needs a time base even when the packets carry no timestamps, because that is the scale
  // the codec expresses frame times in.
  context->time_base = kFrameTimeBase;

  if (avcodec_open2(context.get(), codec, nullptr) < 0) {
    throw DecoderException(DecoderError::CodecOpenFailed, "Failed to open FFmpeg codec '" + codec_hint +
                             "'. Verify the codec is supported by this FFmpeg build.");
  }

  return context;
}

FramePtr allocateFrameBuffer() {
  FramePtr frame(av_frame_alloc());
  if (!frame) {
    throw DecoderException(DecoderError::AllocationFailed, "Failed to allocate FFmpeg AVFrame. The system is out of memory.");
  }
  return frame;
}

PacketPtr allocatePacketBuffer() {
  PacketPtr packet(av_packet_alloc());
  if (!packet) {
    throw DecoderException(DecoderError::AllocationFailed, "Failed to allocate FFmpeg AVPacket. The system is out of memory.");
  }
  return packet;
}

}  // namespace iptv::decoder::internal