#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "iptv/decoder/ffmpeg_decoder.hpp"
#include "iptv/decoder/decoder_error.hpp"
#include "mpeg4_source.hpp"

using namespace iptv::decoder;
using iptv::decoder::testing::encodeMpeg4Pictures;

// Picture size for the round trip tests: mpeg4 wants multiples of 16, and 32x24 keeps a test
// payload to a kilobyte while still having more than one row per plane to miscopy.
constexpr int kPictureWidth = 32;
constexpr int kPictureHeight = 24;

// Pictures per test stream. The count is deliberately larger than a decoder thread count: a
// frame-threaded decoder holds that many pictures back, and the flush below then has to release
// several of them at once.
constexpr int kPictureCount = 8;

/**
 * @brief Mean luma of a decoded frame.
 *
 * The pictures are painted as luma ramps that rise with the picture index, so the mean identifies
 * which picture a frame came from, which a byte for byte comparison cannot do through a lossy
 * codec.
 */
[[nodiscard]] int averageLuma(const DecodedFrame& frame) {
  const std::size_t luma_size =
      static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
  int total = 0;
  for (std::size_t index = 0; index < luma_size; ++index) {
    total += frame.data[index];
  }
  return static_cast<int>(total) / static_cast<int>(luma_size);
}

/**
 * @brief Hands one encoded packet to @p decoder as the span a caller would.
 */
std::vector<DecodedFrame> decodePacket(FFmpegDecoder& decoder, const std::vector<uint8_t>& packet) {
  return decoder.decode(std::span<const uint8_t>{packet});
}

/**
 * @brief Moves the frames of @p from onto the end of @p into.
 *
 * std::vector::insert needs a move-assignable element and a DecodedFrame is move-only, so frames
 * are transferred one at a time instead.
 */
void appendFrames(std::vector<DecodedFrame>& into, std::vector<DecodedFrame>& from) {
  for (DecodedFrame& frame : from) {
    into.emplace_back(std::move(frame));
  }
}

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
  EXPECT_THROW({ (void)source.decode(std::span<const uint8_t>{}); }, std::logic_error);

  // The destination is unaffected by the guard on the source.
  EXPECT_EQ(sink.getCodecInfo().name, "h264");
}

TEST(FFmpegDecoderTest, MovedFromDecoderReacceptsUseAfterReassignment) {
  FFmpegDecoder source("h264");
  FFmpegDecoder sink(std::move(source));

  source = FFmpegDecoder("h264");
  EXPECT_EQ(source.getCodecInfo().name, "h264");
}

TEST(FFmpegDecoderTest, DecodeTurnsAPacketIntoOnePackedYuv420pFrame) {
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, 1);
  ASSERT_EQ(packets.size(), 1U);
  FFmpegDecoder decoder("mpeg4");

  std::vector<DecodedFrame> frames = decodePacket(decoder, packets[0]);

  ASSERT_EQ(frames.size(), 1U);
  const DecodedFrame& frame = frames[0];
  EXPECT_EQ(frame.width, kPictureWidth);
  EXPECT_EQ(frame.height, kPictureHeight);
  EXPECT_EQ(frame.pixel_format, PixelFormat::YUV420P);
  // Luma, then two quarter sized chroma planes, packed with no padding between them.
  EXPECT_EQ(frame.data.size(), static_cast<std::size_t>(kPictureWidth * kPictureHeight) * 3 / 2);
  // Declared strides address the packed payload, which is what the render path walks.
  EXPECT_EQ(frame.linesize[0], kPictureWidth);
  EXPECT_EQ(frame.linesize[1], kPictureWidth / 2);
  EXPECT_EQ(frame.linesize[2], kPictureWidth / 2);
  // The first picture is a ramp over columns 0..31, so its mean luma sits near the middle.
  EXPECT_GT(averageLuma(frame), 8);
  EXPECT_LT(averageLuma(frame), 24);
}

TEST(FFmpegDecoderTest, DecodeCollectsEveryFrameOfAReorderingStreamAndFlushHoldsTheLast) {
  // A codec holding pictures back for reordering is the case where one packet is not one frame:
  // the first packet yields nothing, later packets yield one each, and the last picture only comes
  // out when the decoder is flushed.
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, kPictureCount, 2);
  ASSERT_EQ(packets.size(), static_cast<std::size_t>(kPictureCount));
  FFmpegDecoder decoder("mpeg4");

  std::vector<DecodedFrame> decoded;
  for (const std::vector<uint8_t>& packet : packets) {
    std::vector<DecodedFrame> frames = decodePacket(decoder, packet);
    appendFrames(decoded, frames);
  }
  std::vector<DecodedFrame> flushed = decoder.flush();
  appendFrames(decoded, flushed);

  // Every encoded picture is accounted for, none of them twice.
  ASSERT_EQ(decoded.size(), static_cast<std::size_t>(kPictureCount));
  EXPECT_EQ(averageLuma(decoded[0]), 15);
  EXPECT_EQ(averageLuma(decoded[1]), 23);
  EXPECT_EQ(averageLuma(decoded[2]), 31);
  for (const DecodedFrame& frame : decoded) {
    EXPECT_EQ(frame.width, kPictureWidth);
    EXPECT_EQ(frame.data.size(), static_cast<std::size_t>(kPictureWidth * kPictureHeight) * 3 / 2);
  }

  // Times are synthesized from the 25 frames per second the stream declares, so they march
  // forward one 40 millisecond frame at a time even though the packets carried no timestamps.
  for (std::size_t index = 0; index < decoded.size(); ++index) {
    EXPECT_EQ(decoded[index].pts, static_cast<int64_t>(index) * 40);
    EXPECT_EQ(decoded[index].duration, 40);
  }
}

TEST(FFmpegDecoderTest, RepeatedFlushDrainsNothingFurther) {
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, kPictureCount, 2);
  FFmpegDecoder decoder("mpeg4");
  for (const std::vector<uint8_t>& packet : packets) {
    (void)decodePacket(decoder, packet);
  }

  EXPECT_FALSE(decoder.flush().empty());
  // The first flush closed the codec; draining again has nothing left and must not fail.
  EXPECT_TRUE(decoder.flush().empty());
}

TEST(FFmpegDecoderTest, DecodeOfAnEmptyPacketProducesNothingAndLeavesTheDecoderUsable) {
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, 1);
  ASSERT_EQ(packets.size(), 1U);
  FFmpegDecoder decoder("mpeg4");

  // No bytes is not a picture, and it must not leave the codec in a state that eats the next one.
  EXPECT_TRUE(decoder.decode(std::span<const uint8_t>{}).empty());
  EXPECT_EQ(decodePacket(decoder, packets[0]).size(), 1U);
}

TEST(FFmpegDecoderTest, DecodeDropsACorruptPacketAndStillDecodesTheNextOne) {
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, 1);
  ASSERT_EQ(packets.size(), 1U);
  // A start code followed by noise: shaped like compressed input, meaningless to any codec.
  const std::vector<uint8_t> corrupt(64);
  FFmpegDecoder decoder("mpeg4");

  std::vector<uint8_t> noise = corrupt;
  for (std::size_t index = 0; index < noise.size(); ++index) {
    noise[index] = static_cast<uint8_t>((index * 37) + 11);
  }

  // A corrupt packet is the stream's fault, not a failure of the decoder: it yields no frame and
  // must not take the decoder down with it.
  EXPECT_NO_THROW({ (void)decodePacket(decoder, noise); });
  EXPECT_EQ(decodePacket(decoder, packets[0]).size(), 1U);
}

TEST(FFmpegDecoderTest, DecodeAfterFlushFailsLoudlyInsteadOfDroppingThePacket) {
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, 1);
  ASSERT_EQ(packets.size(), 1U);
  FFmpegDecoder decoder("mpeg4");
  ASSERT_TRUE(decoder.flush().empty());

  // The codec is closed to input, so returning empty here would hide a dropped packet forever.
  EXPECT_THROW({ (void)decodePacket(decoder, packets[0]); }, std::logic_error);
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
  EXPECT_EQ(info.fps, 0.0);
}

TEST(FFmpegDecoderTest, CodecInfoReportsGeometryAndRateOnceTheCodecHasDecoded) {
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, 1);
  ASSERT_EQ(packets.size(), 1U);
  FFmpegDecoder decoder("mpeg4");

  ASSERT_EQ(decodePacket(decoder, packets[0]).size(), 1U);

  // The stream told the codec its geometry and rate; reporting them now beats reporting zeros.
  const CodecInfo info = decoder.getCodecInfo();
  EXPECT_EQ(info.name, "mpeg4");
  EXPECT_EQ(info.width, kPictureWidth);
  EXPECT_EQ(info.height, kPictureHeight);
  EXPECT_DOUBLE_EQ(info.fps, 25.0);
}

TEST(FFmpegDecoderTest, H264DecodeOfMalformedBytesDoesNotCrash) {
  // Deterministic malformed H.264 data: a start code followed by an invalid NAL type.
  // The decoder must reject this gracefully without crashing or throwing.
  FFmpegDecoder decoder("h264");

  // Start code + invalid NAL type
  const std::vector<uint8_t> malformed = {0x00, 0x00, 0x01, 0xFF};
  EXPECT_NO_THROW({ (void)decodePacket(decoder, malformed); });
  EXPECT_TRUE(decodePacket(decoder, malformed).empty());

  // Truncated start code
  const std::vector<uint8_t> truncated = {0x00, 0x00};
  EXPECT_NO_THROW({ (void)decodePacket(decoder, truncated); });

  // Start code only
  const std::vector<uint8_t> start_code_only = {0x00, 0x00, 0x01};
  EXPECT_NO_THROW({ (void)decodePacket(decoder, start_code_only); });

  // Decoder must remain usable after malformed input
  EXPECT_NO_THROW({ (void)decoder.flush(); });
}

TEST(FFmpegDecoderTest, PartialPacketHandlingDoesNotCrash) {
  // A valid packet split into two halves: the decoder must handle partial data gracefully.
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, 1);
  ASSERT_EQ(packets.size(), 1U);

  const std::vector<uint8_t>& full_packet = packets[0];
  ASSERT_GT(full_packet.size(), 2U);

  const std::size_t mid = full_packet.size() / 2;
  const std::vector<uint8_t> first_half(full_packet.begin(), full_packet.begin() + mid);
  const std::vector<uint8_t> second_half(full_packet.begin() + mid, full_packet.end());

  FFmpegDecoder decoder("mpeg4");

  // Feed partial data: first half usually drops
  EXPECT_NO_THROW({ (void)decodePacket(decoder, first_half); });
  
  // Second half might cause FFmpeg to reject the packet depending on header state.
  // The test passes as long as we don't crash (ASan/TSan will catch memory errors).
  try {
    (void)decodePacket(decoder, second_half);
  } catch (const std::exception&) {
    // Expected to potentially fail
  }

  // Flush to ensure decoder remains usable
  try {
    (void)decoder.flush();
  } catch (const std::exception&) {
    // Also might fail if codec is left in invalid state, but must not crash
  }
}

TEST(FFmpegDecoderTest, PtsTrackingWithoutReordering) {
  // With no B-frames, each packet produces one frame immediately.
  // PTS should still be tracked correctly.
  const std::vector<std::vector<uint8_t>> packets =
      encodeMpeg4Pictures(kPictureWidth, kPictureHeight, kPictureCount, 0);
  ASSERT_EQ(packets.size(), static_cast<std::size_t>(kPictureCount));

  FFmpegDecoder decoder("mpeg4");
  std::vector<DecodedFrame> decoded;
  for (const std::vector<uint8_t>& packet : packets) {
    std::vector<DecodedFrame> frames = decodePacket(decoder, packet);
    appendFrames(decoded, frames);
  }

  // With no B-frames, all frames should be decoded without flush
  ASSERT_EQ(decoded.size(), static_cast<std::size_t>(kPictureCount));

  // PTS starts at 0 and advances by duration each frame
  for (std::size_t i = 0; i < decoded.size(); ++i) {
    EXPECT_EQ(decoded[i].pts, static_cast<int64_t>(i) * 40);
    EXPECT_EQ(decoded[i].duration, 40);
  }
}

TEST(FFmpegDecoderTest, DecodeDropsMalformedH264DataAndDoesNotCrash) {
  // Deterministic garbage byte array resembling a malformed NALU.
  const std::vector<uint8_t> garbage = {0x00, 0x00, 0x01, 0xFF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
  FFmpegDecoder decoder("h264");

  // Must not crash or loop infinitely.
  EXPECT_NO_THROW({
    std::vector<DecodedFrame> frames = decodePacket(decoder, garbage);
    EXPECT_TRUE(frames.empty());
  });
}

TEST(FFmpegDecoderTest, DecodeWithShortIncompleteDataFailsFast) {
  // Just a start code and no real data.
  const std::vector<uint8_t> partial = {0x00, 0x00, 0x00, 0x01};
  FFmpegDecoder decoder("h264");

  EXPECT_NO_THROW({
    std::vector<DecodedFrame> frames = decodePacket(decoder, partial);
    EXPECT_TRUE(frames.empty());
  });
}

TEST(FFmpegDecoderTest, CodecInfoWithGarbageReturnsZeros) {
  // Sending invalid data doesn't initialize geometry.
  const std::vector<uint8_t> garbage = {0xFF, 0xFF, 0xFF, 0xFF};
  FFmpegDecoder decoder("h264");
  
  EXPECT_NO_THROW({ (void)decodePacket(decoder, garbage); });
  const CodecInfo info = decoder.getCodecInfo();
  EXPECT_EQ(info.width, 0);
  EXPECT_EQ(info.height, 0);
}
TEST(FFmpegDecoderTest, DecodeThrowsOnOversizedPacket) {
  FFmpegDecoder decoder("mpeg4");
  std::span<const uint8_t> giant_span(static_cast<const uint8_t*>(nullptr), static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1);
  try {
    decoder.decode(giant_span);
    FAIL() << "Expected DecoderException";
  } catch (const DecoderException& e) {
    EXPECT_EQ(e.error(), DecoderError::UnknownError);
  }
}

TEST(FFmpegDecoderTest, AutoDetectThrowsOnUnrecognizedBitstream) {
  FFmpegDecoder decoder(""); // Empty hint enables auto-detect
  const std::vector<uint8_t> garbage = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};
  try {
    decoder.decode(garbage);
    FAIL() << "Expected DecoderException";
  } catch (const DecoderException& e) {
    EXPECT_EQ(e.error(), DecoderError::CodecNotFound);
  }
}

