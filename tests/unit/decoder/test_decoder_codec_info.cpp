#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "decoder/decoder_frame_fixture.hpp"
#include "iptv/decoder/codec_info.hpp"
#include "iptv/decoder/i_audio_decoder.hpp"
#include "iptv/decoder/i_video_decoder.hpp"

using namespace iptv::decoder;
using ::testing::_;
using ::testing::Return;

namespace {

CodecInfo makeVideoCodecInfo() {
  return CodecInfo(
      CodecIdentity{
          .name = "h264", .profile = "High", .bitrate = 4500000, .hardware_accelerated = true},
      VideoCodecParameters{.width = 1920, .height = 1080, .fps = 50.0});
}

CodecInfo makeAudioCodecInfo() {
  return CodecInfo(
      CodecIdentity{
          .name = "aac", .profile = "LC", .bitrate = 128000, .hardware_accelerated = false},
      AudioCodecParameters{.sample_rate = 48000, .channels = 2});
}

}  // namespace

class MockVideoCodecDecoder : public IVideoDecoder {
 public:
  MOCK_METHOD(std::vector<DecodedFrame>, decode, (std::span<const uint8_t>), (override));
  MOCK_METHOD(std::vector<DecodedFrame>, flush, (), (override));
  MOCK_METHOD(CodecInfo, getCodecInfo, (), (const, override));
};

class MockAudioCodecDecoder : public IAudioDecoder {
 public:
  MOCK_METHOD(std::vector<DecodedFrame>, decode, (std::span<const uint8_t>), (override));
  MOCK_METHOD(std::vector<DecodedFrame>, flush, (), (override));
  MOCK_METHOD(CodecInfo, getCodecInfo, (), (const, override));
};

TEST(DecoderCodecInfoTest, VideoDecoderReportsCodecInfo) {
  MockVideoCodecDecoder mock_video;

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

TEST(DecoderCodecInfoTest, AudioDecoderReportsCodecInfo) {
  MockAudioCodecDecoder mock_audio;

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

TEST(DecoderCodecInfoTest, CodecInfoMovesAcrossOwnershipBoundary) {
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

TEST(DecoderCodecInfoTest, VideoCodecInfoConstructorLeavesAudioParametersZeroed) {
  const CodecInfo info = makeVideoCodecInfo();

  // The video constructor takes no audio parameters at all, so the audio group is zero rather than
  // whatever the caller last happened to have lying around.
  EXPECT_EQ(info.sample_rate, 0);
  EXPECT_EQ(info.channels, 0);
}

TEST(DecoderCodecInfoTest, AudioCodecInfoConstructorLeavesVideoParametersZeroed) {
  const CodecInfo info = makeAudioCodecInfo();

  EXPECT_EQ(info.width, 0);
  EXPECT_EQ(info.height, 0);
  EXPECT_DOUBLE_EQ(info.fps, 0.0);
}

TEST(DecoderCodecInfoTest, VideoDecoderFlushReturnsRemainingFrames) {
  MockVideoCodecDecoder mock_video;
  DecodedFrame first = makeVideoFrame({0x0A}, 4000, 1920, 1080);
  DecodedFrame second = makeVideoFrame({0x0B}, 5000, 1920, 1080);

  EXPECT_CALL(mock_video, flush())
      .WillOnce(Return(makeFrames(std::move(first), std::move(second))));

  const std::vector<DecodedFrame> result = mock_video.flush();

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].pts, 4000);
  EXPECT_EQ(result[1].pts, 5000);
}

TEST(DecoderCodecInfoTest, AudioDecoderFlushReturnsRemainingFrames) {
  MockAudioCodecDecoder mock_audio;
  DecodedFrame frame = makeAudioFrame({0xCC}, 4096, 48000, 2);

  EXPECT_CALL(mock_audio, flush()).WillOnce(Return(makeFrames(std::move(frame))));

  const std::vector<DecodedFrame> result = mock_audio.flush();

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].sample_rate, 48000);
  EXPECT_EQ(result[0].channels, 2);
}

TEST(DecoderCodecInfoTest, FlushReturnsEmptyWhenNothingBuffered) {
  MockVideoCodecDecoder mock_video;
  MockAudioCodecDecoder mock_audio;

  EXPECT_CALL(mock_video, flush()).WillOnce(Return(std::vector<DecodedFrame>{}));
  EXPECT_CALL(mock_audio, flush()).WillOnce(Return(std::vector<DecodedFrame>{}));

  EXPECT_TRUE(mock_video.flush().empty());
  EXPECT_TRUE(mock_audio.flush().empty());
}
