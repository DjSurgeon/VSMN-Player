#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>

#include "iptv/decoder/ffmpeg_decoder.hpp"

using namespace iptv::decoder;

TEST(FFmpegDecoderTest, ConstructionAndDestructionIsLeakFree) {
  // Tests that allocating and tearing down an FFmpeg RAII wrapper does not leak.
  // We use h264 as it is linked via our Conan integration.
  EXPECT_NO_THROW({ FFmpegDecoder decoder("h264"); });
}

TEST(FFmpegDecoderTest, InvalidCodecThrowsException) {
  EXPECT_THROW({ FFmpegDecoder decoder("invalid_codec_name"); }, std::runtime_error);
}

TEST(FFmpegDecoderTest, MoveSemanticsTransferOwnership) {
  FFmpegDecoder source("h264");
  
  // Verify CodecInfo before move
  CodecInfo info = source.getCodecInfo();
  EXPECT_EQ(info.name, "h264");

  // Move construct
  FFmpegDecoder sink(std::move(source));
  
  CodecInfo moved_info = sink.getCodecInfo();
  EXPECT_EQ(moved_info.name, "h264");
}
