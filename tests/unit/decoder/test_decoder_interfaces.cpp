#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "iptv/decoder/codec_info.hpp"
#include "iptv/decoder/decoded_frame.hpp"
#include "iptv/decoder/i_audio_decoder.hpp"
#include "iptv/decoder/i_video_decoder.hpp"

using namespace iptv::decoder;
using ::testing::_;
using ::testing::Return;
using ::testing::Truly;

// Move-only proof: neither a decoded frame nor a codec info may ever be deep-copied. Both types
// delete their copy operations, so an accidental copy on the render hot path is a compile error
// rather than a hidden allocation. Move stays usable, which is what lets media flow between the
// decode, queue, and render threads by hand-off.
static_assert(!std::is_copy_constructible_v<DecodedFrame>,
              "DecodedFrame must be move-only: sample payloads are handed over, never copied");
static_assert(!std::is_copy_assignable_v<DecodedFrame>,
              "DecodedFrame must be move-only: sample payloads are handed over, never copied");
static_assert(std::is_move_constructible_v<DecodedFrame>,
              "DecodedFrame must be movable so frames travel between threads by hand-off");
static_assert(std::is_move_assignable_v<DecodedFrame>,
              "DecodedFrame must be move-assignable so a buffer can be refilled in place");

static_assert(!std::is_copy_constructible_v<CodecInfo>, "CodecInfo must be move-only");
static_assert(!std::is_copy_assignable_v<CodecInfo>, "CodecInfo must be move-only");
static_assert(std::is_move_constructible_v<CodecInfo>,
              "CodecInfo must be movable so it can cross thread boundaries by hand-off");
static_assert(std::is_move_assignable_v<CodecInfo>, "CodecInfo must be move-assignable");

namespace {

/**
 * @brief Builds a planar YUV420P video frame with coherent plane strides.
 *
 * DecodedFrame is not an aggregate (its copy operations are user-declared), so frames are filled
 * field by field rather than brace-initialised. Strides are derived from the geometry, which is
 * exactly the invariant the render path relies on: chroma is half resolution in both axes.
 */
DecodedFrame makeVideoFrame(std::vector<uint8_t> payload, int64_t pts, int width, int height,
                            int64_t duration = 3000) {
  DecodedFrame frame;
  frame.data = std::move(payload);
  frame.pts = pts;
  frame.duration = duration;
  frame.media_type = MediaType::Video;
  frame.width = width;
  frame.height = height;
  frame.pixel_format = PixelFormat::YUV420P;
  frame.linesize = {width, width / 2, width / 2, 0};
  return frame;
}

/**
 * @brief Builds a signed 16-bit PCM audio frame, deriving the declared format from the rate.
 */
DecodedFrame makeAudioFrame(std::vector<uint8_t> payload, int64_t pts, int sample_rate,
                            int channels, int64_t duration = 1024) {
  DecodedFrame frame;
  frame.data = std::move(payload);
  frame.pts = pts;
  frame.duration = duration;
  frame.media_type = MediaType::Audio;
  frame.sample_rate = sample_rate;
  frame.channels = channels;
  frame.audio_format =
      sample_rate == 48000 ? AudioFormat::PCM_S16_48KHZ : AudioFormat::PCM_S16_44KHZ;
  return frame;
}

CodecInfo makeVideoCodecInfo() {
  CodecInfo info;
  info.name = "h264";
  info.profile = "High";
  info.bitrate = 4500000;
  info.hardware_accelerated = true;
  info.width = 1920;
  info.height = 1080;
  info.fps = 50.0;
  return info;
}

CodecInfo makeAudioCodecInfo() {
  CodecInfo info;
  info.name = "aac";
  info.profile = "LC";
  info.bitrate = 128000;
  info.hardware_accelerated = false;
  info.sample_rate = 48000;
  info.channels = 2;
  return info;
}

/**
 * @brief Builds a frame vector by moving each frame in.
 *
 * std::vector's initializer_list constructor requires a copy-constructible element type, so with a
 * move-only frame the frames have to be emplaced one by one.
 */
template <typename... Frames>
std::vector<DecodedFrame> makeFrames(Frames&&... frames) {
  std::vector<DecodedFrame> out;
  out.reserve(sizeof...(Frames));
  (out.emplace_back(std::forward<Frames>(frames)), ...);
  return out;
}

}  // namespace

class MockVideoDecoder : public IVideoDecoder {
 public:
  MOCK_METHOD(std::vector<DecodedFrame>, decode, (std::span<const uint8_t>), (override));
  MOCK_METHOD(std::vector<DecodedFrame>, flush, (), (override));
  MOCK_METHOD(CodecInfo, getCodecInfo, (), (const, override));
};

class MockAudioDecoder : public IAudioDecoder {
 public:
  MOCK_METHOD(std::vector<DecodedFrame>, decode, (std::span<const uint8_t>), (override));
  MOCK_METHOD(std::vector<DecodedFrame>, flush, (), (override));
  MOCK_METHOD(CodecInfo, getCodecInfo, (), (const, override));
};

TEST(DecoderInterfacesTest, VideoDecoderMocking) {
  MockVideoDecoder mock_video;
  DecodedFrame dummy_frame = makeVideoFrame({0x00, 0xFF}, 1000, 1920, 1080);

  EXPECT_CALL(mock_video, decode(_)).WillOnce(Return(makeFrames(std::move(dummy_frame))));

  const std::vector<uint8_t> packet = {0x12, 0x34};
  const std::vector<DecodedFrame> result = mock_video.decode(std::span<const uint8_t>{packet});

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].media_type, MediaType::Video);
  EXPECT_EQ(result[0].width, 1920);
  EXPECT_EQ(result[0].height, 1080);
  EXPECT_EQ(result[0].data.size(), 2U);
}

TEST(DecoderInterfacesTest, AudioDecoderMocking) {
  MockAudioDecoder mock_audio;
  DecodedFrame dummy_frame = makeAudioFrame({0xAA, 0xBB}, 1000, 48000, 2);

  EXPECT_CALL(mock_audio, decode(_)).WillOnce(Return(makeFrames(std::move(dummy_frame))));

  const std::vector<uint8_t> packet = {0x01, 0x02};
  const std::vector<DecodedFrame> result = mock_audio.decode(std::span<const uint8_t>{packet});

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].media_type, MediaType::Audio);
  EXPECT_EQ(result[0].sample_rate, 48000);
  EXPECT_EQ(result[0].channels, 2);
  EXPECT_EQ(result[0].audio_format, AudioFormat::PCM_S16_48KHZ);
}

TEST(DecoderInterfacesTest, VideoDecoderReturnsMultipleFramesInOrder) {
  MockVideoDecoder mock_video;
  DecodedFrame first = makeVideoFrame({0x01}, 1000, 1920, 1080);
  DecodedFrame second = makeVideoFrame({0x02}, 2000, 1280, 720);
  DecodedFrame third = makeVideoFrame({0x03}, 3000, 640, 480);

  EXPECT_CALL(mock_video, decode(_))
      .WillOnce(Return(makeFrames(std::move(first), std::move(second), std::move(third))));

  const std::vector<uint8_t> packet = {0x12, 0x34, 0x56};
  const std::vector<DecodedFrame> result = mock_video.decode(std::span<const uint8_t>{packet});

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result[0].pts, 1000);
  EXPECT_EQ(result[1].pts, 2000);
  EXPECT_EQ(result[2].pts, 3000);
  EXPECT_EQ(result[0].width, 1920);
  EXPECT_EQ(result[1].width, 1280);
  EXPECT_EQ(result[2].width, 640);
}

TEST(DecoderInterfacesTest, AudioDecoderReturnsMultipleFramesInOrder) {
  MockAudioDecoder mock_audio;
  DecodedFrame first = makeAudioFrame({0xAA}, 1000, 44100, 1);
  DecodedFrame second = makeAudioFrame({0xBB}, 1024, 44100, 2);

  EXPECT_CALL(mock_audio, decode(_))
      .WillOnce(Return(makeFrames(std::move(first), std::move(second))));

  const std::vector<uint8_t> packet = {0x01, 0x02};
  const std::vector<DecodedFrame> result = mock_audio.decode(std::span<const uint8_t>{packet});

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].sample_rate, 44100);
  EXPECT_EQ(result[0].channels, 1);
  EXPECT_EQ(result[0].audio_format, AudioFormat::PCM_S16_44KHZ);
  EXPECT_EQ(result[1].sample_rate, 44100);
  EXPECT_EQ(result[1].channels, 2);
}

TEST(DecoderInterfacesTest, VideoFrameTracksPlaneStridesForYuv420p) {
  constexpr int kWidth = 1920;
  constexpr int kHeight = 1080;
  constexpr int kChromaWidth = kWidth / 2;

  DecodedFrame frame;
  frame.media_type = MediaType::Video;
  frame.width = kWidth;
  frame.height = kHeight;
  frame.pixel_format = PixelFormat::YUV420P;
  frame.linesize = {kWidth, kChromaWidth, kChromaWidth, 0};

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

TEST(DecoderInterfacesTest, DefaultConstructedFrameClaimsNoMediaLayout) {
  const DecodedFrame frame;

  EXPECT_EQ(frame.pts, 0);
  EXPECT_EQ(frame.duration, 0);
  EXPECT_EQ(frame.pixel_format, PixelFormat::Unknown);
  EXPECT_EQ(frame.audio_format, AudioFormat::Unknown);
  EXPECT_EQ(frame.linesize[0], 0);
  EXPECT_TRUE(frame.data.empty());
}

TEST(DecoderInterfacesTest, MoveConstructionTransfersPayloadWithoutCopying) {
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

TEST(DecoderInterfacesTest, MoveAssignmentReplacesFrameInPlace) {
  DecodedFrame source = makeAudioFrame({0x01, 0x02, 0x03, 0x04}, 9000, 48000, 2);
  DecodedFrame sink = makeVideoFrame(std::vector<uint8_t>(8U, 0xFFU), 1, 1920, 1080);

  sink = std::move(source);

  EXPECT_EQ(sink.pts, 9000);
  EXPECT_EQ(sink.media_type, MediaType::Audio);
  EXPECT_EQ(sink.sample_rate, 48000);
  EXPECT_EQ(sink.channels, 2);
  EXPECT_EQ(sink.audio_format, AudioFormat::PCM_S16_48KHZ);
  EXPECT_EQ(sink.data.size(), 4U);

  // Video-only fields of the overwritten frame must not leak into the audio frame.
  EXPECT_EQ(sink.width, 0);
  EXPECT_EQ(sink.height, 0);
}

TEST(DecoderInterfacesTest, VideoDecoderReportsCodecInfo) {
  MockVideoDecoder mock_video;

  EXPECT_CALL(mock_video, getCodecInfo()).WillOnce(Return(makeVideoCodecInfo()));

  const CodecInfo result = mock_video.getCodecInfo();

  EXPECT_EQ(result.name, "h264");
  EXPECT_EQ(result.profile, "High");
  EXPECT_EQ(result.bitrate, 4500000);
  EXPECT_TRUE(result.hardware_accelerated);
  EXPECT_EQ(result.width, 1920);
  EXPECT_EQ(result.height, 1080);
  EXPECT_DOUBLE_EQ(result.fps, 50.0);
  EXPECT_EQ(result.sample_rate, 0);
  EXPECT_EQ(result.channels, 0);
}

TEST(DecoderInterfacesTest, AudioDecoderReportsCodecInfo) {
  MockAudioDecoder mock_audio;

  EXPECT_CALL(mock_audio, getCodecInfo()).WillOnce(Return(makeAudioCodecInfo()));

  const CodecInfo result = mock_audio.getCodecInfo();

  EXPECT_EQ(result.name, "aac");
  EXPECT_EQ(result.profile, "LC");
  EXPECT_EQ(result.bitrate, 128000);
  EXPECT_FALSE(result.hardware_accelerated);
  EXPECT_EQ(result.sample_rate, 48000);
  EXPECT_EQ(result.channels, 2);
  EXPECT_EQ(result.width, 0);
  EXPECT_DOUBLE_EQ(result.fps, 0.0);
}

TEST(DecoderInterfacesTest, CodecInfoMovesAcrossOwnershipBoundary) {
  CodecInfo source = makeVideoCodecInfo();

  CodecInfo sink(std::move(source));
  EXPECT_EQ(sink.name, "h264");
  EXPECT_EQ(sink.width, 1920);

  CodecInfo replacement = makeAudioCodecInfo();
  sink = std::move(replacement);
  EXPECT_EQ(sink.name, "aac");
  EXPECT_EQ(sink.sample_rate, 48000);
  EXPECT_EQ(sink.width, 0);
}

TEST(DecoderInterfacesTest, DecodeReturnsEmptyWhenNoFrameProduced) {
  MockVideoDecoder mock_video;
  MockAudioDecoder mock_audio;

  EXPECT_CALL(mock_video, decode(_)).WillOnce(Return(std::vector<DecodedFrame>{}));
  EXPECT_CALL(mock_audio, decode(_)).WillOnce(Return(std::vector<DecodedFrame>{}));

  const std::vector<uint8_t> packet = {0x00};

  EXPECT_TRUE(mock_video.decode(std::span<const uint8_t>{packet}).empty());
  EXPECT_TRUE(mock_audio.decode(std::span<const uint8_t>{packet}).empty());
}

TEST(DecoderInterfacesTest, DecodeSeesSpanOverContiguousBytesWithoutCopying) {
  MockVideoDecoder mock_video;
  MockAudioDecoder mock_audio;

  // A span is constructible from any contiguous byte source, not just std::vector.
  constexpr uint8_t kRawPacket[] = {0x12, 0x34, 0x56, 0x78};
  const std::span<const uint8_t> packet(kRawPacket);

  const std::vector<uint8_t> expected = {0x12, 0x34, 0x56, 0x78};
  const auto matches_packet = [&expected](std::span<const uint8_t> received) {
    return received.size() == expected.size() &&
           std::equal(received.begin(), received.end(), expected.begin());
  };

  EXPECT_CALL(mock_video, decode(Truly(matches_packet)))
      .WillOnce(Return(std::vector<DecodedFrame>{}));
  EXPECT_CALL(mock_audio, decode(Truly(matches_packet)))
      .WillOnce(Return(std::vector<DecodedFrame>{}));

  EXPECT_TRUE(mock_video.decode(packet).empty());
  EXPECT_TRUE(mock_audio.decode(packet).empty());
}

TEST(DecoderInterfacesTest, VideoDecoderFlushReturnsRemainingFrames) {
  MockVideoDecoder mock_video;
  DecodedFrame first = makeVideoFrame({0x0A}, 4000, 1920, 1080);
  DecodedFrame second = makeVideoFrame({0x0B}, 5000, 1920, 1080);

  EXPECT_CALL(mock_video, flush())
      .WillOnce(Return(makeFrames(std::move(first), std::move(second))));

  const std::vector<DecodedFrame> result = mock_video.flush();

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].pts, 4000);
  EXPECT_EQ(result[1].pts, 5000);
}

TEST(DecoderInterfacesTest, AudioDecoderFlushReturnsRemainingFrames) {
  MockAudioDecoder mock_audio;
  DecodedFrame frame = makeAudioFrame({0xCC}, 4096, 48000, 2);

  EXPECT_CALL(mock_audio, flush()).WillOnce(Return(makeFrames(std::move(frame))));

  const std::vector<DecodedFrame> result = mock_audio.flush();

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].sample_rate, 48000);
  EXPECT_EQ(result[0].channels, 2);
}

TEST(DecoderInterfacesTest, FlushReturnsEmptyWhenNothingBuffered) {
  MockVideoDecoder mock_video;
  MockAudioDecoder mock_audio;

  EXPECT_CALL(mock_video, flush()).WillOnce(Return(std::vector<DecodedFrame>{}));
  EXPECT_CALL(mock_audio, flush()).WillOnce(Return(std::vector<DecodedFrame>{}));

  EXPECT_TRUE(mock_video.flush().empty());
  EXPECT_TRUE(mock_audio.flush().empty());
}
