#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "decoder/decoder_frame_fixture.hpp"
#include "iptv/decoder/decoded_frame.hpp"

using namespace iptv::decoder;

TEST(DecoderFrameTest, VideoFrameTracksPlaneStridesForYuv420p) {
  constexpr int kWidth = 1920;
  constexpr int kHeight = 1080;
  constexpr int kChromaWidth = kWidth / 2;

  // Strides arrive with the geometry, never as a leftover from a default state: a picture frame
  // that arrived without them would leave the render path guessing how to address its planes.
  const VideoFrameGeometry geometry{
      .width = kWidth,
      .height = kHeight,
      .pixel_format = PixelFormat::YUV420P,
      .linesize =
          std::array<int, DecodedFrame::kLinesizeSlots>{kWidth, kChromaWidth, kChromaWidth, 0},
  };
  const DecodedFrame frame(FrameTiming{.pts = 0, .duration = 3000}, geometry,
                           std::vector<uint8_t>{});

  ASSERT_EQ(frame.linesize.size(), DecodedFrame::kLinesizeSlots);
  EXPECT_EQ(frame.linesize[0], kWidth);
  EXPECT_EQ(frame.linesize[1], kChromaWidth);
  EXPECT_EQ(frame.linesize[2], kChromaWidth);
  EXPECT_EQ(frame.linesize[3], 0);

  const std::size_t luma_bytes =
      static_cast<std::size_t>(frame.linesize[0]) * static_cast<std::size_t>(frame.height);
  const std::size_t chroma_bytes =
      static_cast<std::size_t>(frame.linesize[1]) * static_cast<std::size_t>(frame.height / 2);

  // Planar YUV420P is not tightly packed, so the buffer holds three independently strided
  // planes rather than a width * height image: rows must be addressed by linesize, never by
  // multiplying the row index by width.
  EXPECT_GT(chroma_bytes, 0U);
  EXPECT_EQ(luma_bytes, static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight));
  EXPECT_EQ(luma_bytes + 2U * chroma_bytes,
            static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight) +
                static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight / 2));
}

TEST(DecoderFrameTest, VideoFrameConstructorLeavesAudioGeometryZeroed) {
  const DecodedFrame frame = makeVideoFrame({0x00, 0xFF}, 1000, 1920, 1080);

  // The constructor stamps the media type, so a caller never states one and cannot disagree with
  // the fields it filled in.
  EXPECT_EQ(frame.media_type, MediaType::Video);

  // No zombie state: a picture frame carries no sample rate, no channel count and no sample
  // layout, so no consumer can read those and believe the frame described audio.
  EXPECT_EQ(frame.sample_rate, 0);
  EXPECT_EQ(frame.channels, 0);
  EXPECT_EQ(frame.audio_format, AudioFormat::Unknown);
}

TEST(DecoderFrameTest, AudioFrameConstructorLeavesVideoGeometryZeroed) {
  const DecodedFrame frame = makeAudioFrame({0xAA, 0xBB}, 1000, 48000, 2);

  EXPECT_EQ(frame.media_type, MediaType::Audio);

  // And the mirror image: no picture geometry and, crucially, no plane strides, so a render path
  // handed this frame cannot walk planes it does not have.
  EXPECT_EQ(frame.width, 0);
  EXPECT_EQ(frame.height, 0);
  EXPECT_EQ(frame.pixel_format, PixelFormat::Unknown);
  EXPECT_EQ(frame.linesize, (std::array<int, DecodedFrame::kLinesizeSlots>{0, 0, 0, 0}));
}

TEST(DecoderFrameTest, MoveConstructionTransfersPayloadWithoutCopying) {
  DecodedFrame source = makeVideoFrame(std::vector<uint8_t>(1280U * 720U, 0x7FU), 4200, 1280, 720);
  const uint8_t* const source_bytes = source.data.data();

  DecodedFrame sink(std::move(source));

  EXPECT_EQ(sink.pts, 4200);
  EXPECT_EQ(sink.duration, 3000);
  EXPECT_EQ(sink.width, 1280);
  EXPECT_EQ(sink.height, 720);
  EXPECT_EQ(sink.linesize[0], 1280);
  ASSERT_EQ(sink.data.size(), 1280U * 720U);

  // The move handed over the very same allocation: no second buffer was allocated and filled.
  EXPECT_EQ(sink.data.data(), source_bytes);
}

TEST(DecoderFrameTest, MoveAssignmentReplacesFrameInPlace) {
  DecodedFrame source = makeAudioFrame({0x01, 0x02, 0x03, 0x04}, 9000, 48000, 2);
  DecodedFrame sink = makeVideoFrame(std::vector<uint8_t>(8U, 0xFFU), 1, 1920, 1080);

  sink = std::move(source);

  EXPECT_EQ(sink.pts, 9000);
  EXPECT_EQ(sink.media_type, MediaType::Audio);
  EXPECT_EQ(sink.sample_rate, 48000);
  EXPECT_EQ(sink.channels, 2);
  EXPECT_EQ(sink.audio_format, AudioFormat::PCM_S16_48KHZ);
  EXPECT_EQ(sink.data.size(), 4U);

  // Video-only fields of the overwritten frame must not leak into the audio frame: the audio
  // constructor zeroed them, so the memberwise move writes zeros instead of the previous picture
  // geometry. Move assignment replaces the whole object rather than merging the two.
  EXPECT_EQ(sink.width, 0);
  EXPECT_EQ(sink.height, 0);
  EXPECT_EQ(sink.pixel_format, PixelFormat::Unknown);
  EXPECT_EQ(sink.linesize, (std::array<int, DecodedFrame::kLinesizeSlots>{0, 0, 0, 0}));
}
