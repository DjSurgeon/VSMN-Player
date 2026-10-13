#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "decoder/decoder_frame_fixture.hpp"
#include "iptv/decoder/codec_info.hpp"
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

// Strict-partition proof: a frame or codec description is always a fully specified video object or
// a fully specified audio object. Without this, `DecodedFrame frame;` yields a half-formed object
// with no picture geometry, no strides and no payload, and a `media_type` of Video that a decoder
// never actually produced - a zombie state the render path has to defend against at every use.
static_assert(!std::is_default_constructible_v<DecodedFrame>,
              "DecodedFrame must not be default constructible: it cannot know which media type "
              "it is without being told");
static_assert(!std::is_default_constructible_v<CodecInfo>,
              "CodecInfo must not be default constructible: an empty codec name describes "
              "nothing");

// Exactly two shapes of each type, one per media type. There is no overload carrying a MediaType
// or a CodecInfo media discriminator: the constructor chosen *is* the declaration of which
// parameter group applies, so a video frame cannot be given audio properties and vice versa.
static_assert(
    std::is_constructible_v<DecodedFrame, FrameTiming, VideoFrameGeometry, std::vector<uint8_t>>,
    "DecodedFrame needs a video constructor taking timing, geometry and payload");
static_assert(
    std::is_constructible_v<DecodedFrame, FrameTiming, AudioFrameGeometry, std::vector<uint8_t>>,
    "DecodedFrame needs an audio constructor taking timing, sample layout and payload");
static_assert(std::is_constructible_v<CodecInfo, CodecIdentity, VideoCodecParameters>,
              "CodecInfo needs a video constructor taking identity and picture parameters");
static_assert(std::is_constructible_v<CodecInfo, CodecIdentity, AudioCodecParameters>,
              "CodecInfo needs an audio constructor taking identity and sample parameters");

// Zero-copy proof: the payload is only ever taken by rvalue reference, so a decoder cannot hand
// over a buffer it still needs and cannot accidentally make a frame deep-copy its samples.
static_assert(
    !std::is_constructible_v<DecodedFrame, FrameTiming, VideoFrameGeometry, std::vector<uint8_t>&>,
    "DecodedFrame must take its payload by move, not bind a caller's buffer by "
    "reference");
static_assert(
    !std::is_constructible_v<DecodedFrame, FrameTiming, AudioFrameGeometry, std::vector<uint8_t>&>,
    "DecodedFrame must take its payload by move, not bind a caller's buffer by "
    "reference");

class MockVideoDecoder : public IVideoDecoder {
 public:
  MOCK_METHOD(std::vector<DecodedFrame>, decode, (std::span<const uint8_t>, int64_t, int64_t), (override));
  MOCK_METHOD(std::vector<DecodedFrame>, flush, (), (override));
  MOCK_METHOD(CodecInfo, getCodecInfo, (), (const, override));
};

class MockAudioDecoder : public IAudioDecoder {
 public:
  MOCK_METHOD(std::vector<DecodedFrame>, decode, (std::span<const uint8_t>, int64_t, int64_t), (override));
  MOCK_METHOD(std::vector<DecodedFrame>, flush, (), (override));
  MOCK_METHOD(CodecInfo, getCodecInfo, (), (const, override));
};

TEST(DecoderInterfacesTest, VideoDecoderMocking) {
  MockVideoDecoder mock_video;
  DecodedFrame dummy_frame = makeVideoFrame({0x00, 0xFF}, 1000, 1920, 1080);

  EXPECT_CALL(mock_video, decode(_, _, _)).WillOnce(Return(makeFrames(std::move(dummy_frame))));

  const std::vector<uint8_t> packet = {0x12, 0x34};
  const std::vector<DecodedFrame> result = mock_video.decode(std::span<const uint8_t>{packet}, -1, -1);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].media_type, MediaType::Video);
  EXPECT_EQ(result[0].width, 1920);
  EXPECT_EQ(result[0].height, 1080);
  EXPECT_EQ(result[0].data.size(), 2U);
}

TEST(DecoderInterfacesTest, AudioDecoderMocking) {
  MockAudioDecoder mock_audio;
  DecodedFrame dummy_frame = makeAudioFrame({0xAA, 0xBB}, 1000, 48000, 2);

  EXPECT_CALL(mock_audio, decode(_, _, _)).WillOnce(Return(makeFrames(std::move(dummy_frame))));

  const std::vector<uint8_t> packet = {0x01, 0x02};
  const std::vector<DecodedFrame> result = mock_audio.decode(std::span<const uint8_t>{packet}, -1, -1);

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

  EXPECT_CALL(mock_video, decode(_, _, _))
      .WillOnce(Return(makeFrames(std::move(first), std::move(second), std::move(third))));

  const std::vector<uint8_t> packet = {0x12, 0x34, 0x56};
  const std::vector<DecodedFrame> result = mock_video.decode(std::span<const uint8_t>{packet}, -1, -1);

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

  EXPECT_CALL(mock_audio, decode(_, _, _))
      .WillOnce(Return(makeFrames(std::move(first), std::move(second))));

  const std::vector<uint8_t> packet = {0x01, 0x02};
  const std::vector<DecodedFrame> result = mock_audio.decode(std::span<const uint8_t>{packet}, -1, -1);

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].sample_rate, 44100);
  EXPECT_EQ(result[0].channels, 1);
  EXPECT_EQ(result[0].audio_format, AudioFormat::PCM_S16_44KHZ);
  EXPECT_EQ(result[1].sample_rate, 44100);
  EXPECT_EQ(result[1].channels, 2);
}

TEST(DecoderInterfacesTest, DecodeReturnsEmptyWhenNoFrameProduced) {
  MockVideoDecoder mock_video;
  MockAudioDecoder mock_audio;

  EXPECT_CALL(mock_video, decode(_, _, _)).WillOnce(Return(std::vector<DecodedFrame>{}));
  EXPECT_CALL(mock_audio, decode(_, _, _)).WillOnce(Return(std::vector<DecodedFrame>{}));

  const std::vector<uint8_t> packet = {0x00};

  EXPECT_TRUE(mock_video.decode(std::span<const uint8_t>{packet}, -1, -1).empty());
  EXPECT_TRUE(mock_audio.decode(std::span<const uint8_t>{packet}, -1, -1).empty());
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

  EXPECT_CALL(mock_video, decode(Truly(matches_packet), _, _))
      .WillOnce(Return(std::vector<DecodedFrame>{}));
  EXPECT_CALL(mock_audio, decode(Truly(matches_packet), _, _))
      .WillOnce(Return(std::vector<DecodedFrame>{}));

  EXPECT_TRUE(mock_video.decode(packet, -1, -1).empty());
  EXPECT_TRUE(mock_audio.decode(packet, -1, -1).empty());
}
