#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

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

TEST(FFmpegDecoderTest, MovedFromDecoderRejectsUseInsteadOfDereferencingNull) {
  FFmpegDecoder source("h264");
  FFmpegDecoder sink(std::move(source));

  // A moved-from decoder owns nothing. Using it must fail loudly, never dereference null.
  EXPECT_THROW({ (void)source.getCodecInfo(); }, std::logic_error);
  EXPECT_THROW({ (void)source.flush(); }, std::logic_error);

  // The destination is unaffected by the guard on the source.
  EXPECT_EQ(sink.getCodecInfo().name, "h264");
}

TEST(FFmpegDecoderTest, MovedFromDecoderReacceptsUseAfterReassignment) {
  FFmpegDecoder source("h264");
  FFmpegDecoder sink(std::move(source));

  source = FFmpegDecoder("h264");
  EXPECT_EQ(source.getCodecInfo().name, "h264");
}

TEST(FFmpegDecoderTest, DecodeReportsUnimplementedPathRatherThanDroppingData) {
  FFmpegDecoder decoder("h264");
  const std::vector<uint8_t> packet{0x00, 0x00, 0x00, 0x01, 0x65};

  // Returning empty would be indistinguishable from a packet that yields no frame,
  // which silently loses video. The call must surface as a failure instead.
  EXPECT_THROW({ (void)decoder.decode(std::span<const uint8_t>{packet}); }, std::logic_error);
}

TEST(FFmpegDecoderTest, DecodeRejectsEmptyInputWithSameExplicitFailure) {
  FFmpegDecoder decoder("h264");

  // Edge case: empty input still reaches the unimplemented path and fails the same way,
  // so callers cannot mistake an empty buffer for a successfully decoded packet.
  EXPECT_THROW({ (void)decoder.decode(std::span<const uint8_t>{}); }, std::logic_error);
}

TEST(FFmpegDecoderTest, FlushOnFreshDecoderReturnsNoFrames) {
  FFmpegDecoder decoder("h264");

  // Nothing has been decoded, so draining is legitimately empty rather than an error.
  EXPECT_TRUE(decoder.flush().empty());
}

TEST(FFmpegDecoderTest, CodecInfoReportsZeroDimensionsUntilCodecIsFed) {
  FFmpegDecoder decoder("h264");

  const CodecInfo info = decoder.getCodecInfo();
  EXPECT_EQ(info.name, "h264");
  EXPECT_EQ(info.width, 0);
  EXPECT_EQ(info.height, 0);
}
