#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include "iptv/decoder/i_audio_decoder.hpp"
#include "iptv/decoder/i_video_decoder.hpp"

using namespace iptv::decoder;
using ::testing::_;
using ::testing::Return;
using ::testing::Truly;

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
  DecodedFrame dummy_frame{{0x00, 0xFF}, 1000, 1000, 1920, 1080, 0, 0};

  EXPECT_CALL(mock_video, decode(_)).WillOnce(Return(std::vector<DecodedFrame>{dummy_frame}));

  const std::vector<uint8_t> packet = {0x12, 0x34};
  const std::vector<DecodedFrame> result = mock_video.decode(std::span<const uint8_t>{packet});

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].width, 1920);
  EXPECT_EQ(result[0].height, 1080);
  EXPECT_EQ(result[0].raw_data.size(), 2U);
}

TEST(DecoderInterfacesTest, AudioDecoderMocking) {
  MockAudioDecoder mock_audio;
  DecodedFrame dummy_frame{{0xAA, 0xBB}, 1000, 1000, 0, 0, 48000, 2};

  EXPECT_CALL(mock_audio, decode(_)).WillOnce(Return(std::vector<DecodedFrame>{dummy_frame}));

  const std::vector<uint8_t> packet = {0x01, 0x02};
  const std::vector<DecodedFrame> result = mock_audio.decode(std::span<const uint8_t>{packet});

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].sample_rate, 48000);
  EXPECT_EQ(result[0].channels, 2);
}

TEST(DecoderInterfacesTest, VideoDecoderReturnsMultipleFramesInOrder) {
  MockVideoDecoder mock_video;
  const DecodedFrame first{{0x01}, 1000, 1000, 1920, 1080, 0, 0};
  const DecodedFrame second{{0x02}, 2000, 2000, 1280, 720, 0, 0};
  const DecodedFrame third{{0x03}, 3000, 3000, 640, 480, 0, 0};

  EXPECT_CALL(mock_video, decode(_))
      .WillOnce(Return(std::vector<DecodedFrame>{first, second, third}));

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
  const DecodedFrame first{{0xAA}, 1000, 1000, 0, 0, 44100, 1};
  const DecodedFrame second{{0xBB}, 1024, 1024, 0, 0, 44100, 2};

  EXPECT_CALL(mock_audio, decode(_)).WillOnce(Return(std::vector<DecodedFrame>{first, second}));

  const std::vector<uint8_t> packet = {0x01, 0x02};
  const std::vector<DecodedFrame> result = mock_audio.decode(std::span<const uint8_t>{packet});

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].sample_rate, 44100);
  EXPECT_EQ(result[0].channels, 1);
  EXPECT_EQ(result[1].sample_rate, 44100);
  EXPECT_EQ(result[1].channels, 2);
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
  const DecodedFrame first{{0x0A}, 4000, 4000, 1920, 1080, 0, 0};
  const DecodedFrame second{{0x0B}, 5000, 5000, 1920, 1080, 0, 0};

  EXPECT_CALL(mock_video, flush()).WillOnce(Return(std::vector<DecodedFrame>{first, second}));

  const std::vector<DecodedFrame> result = mock_video.flush();

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].pts, 4000);
  EXPECT_EQ(result[1].pts, 5000);
}

TEST(DecoderInterfacesTest, AudioDecoderFlushReturnsRemainingFrames) {
  MockAudioDecoder mock_audio;
  const DecodedFrame frame{{0xCC}, 4096, 4096, 0, 0, 48000, 2};

  EXPECT_CALL(mock_audio, flush()).WillOnce(Return(std::vector<DecodedFrame>{frame}));

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
