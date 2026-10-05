#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "iptv/decoder/i_audio_decoder.hpp"
#include "iptv/decoder/i_video_decoder.hpp"

using namespace iptv::decoder;
using ::testing::_;
using ::testing::Return;

class MockVideoDecoder : public IVideoDecoder {
 public:
  MOCK_METHOD(DecodedFrame, decode, (const std::vector<uint8_t>&), (override));
  MOCK_METHOD(std::optional<DecodedFrame>, flush, (), (override));
  MOCK_METHOD(CodecInfo, getCodecInfo, (), (const, override));
};

class MockAudioDecoder : public IAudioDecoder {
 public:
  MOCK_METHOD(DecodedFrame, decode, (const std::vector<uint8_t>&), (override));
  MOCK_METHOD(std::optional<DecodedFrame>, flush, (), (override));
  MOCK_METHOD(CodecInfo, getCodecInfo, (), (const, override));
};

TEST(DecoderInterfacesTest, VideoDecoderMocking) {
  MockVideoDecoder mock_video;
  DecodedFrame dummy_frame{{0x00, 0xFF}, 1000, 1000, 1920, 1080, 0, 0};

  EXPECT_CALL(mock_video, decode(_)).WillOnce(Return(dummy_frame));

  std::vector<uint8_t> packet = {0x12, 0x34};
  auto result = mock_video.decode(packet);

  EXPECT_EQ(result.width, 1920);
  EXPECT_EQ(result.height, 1080);
  EXPECT_EQ(result.raw_data.size(), 2);
}

TEST(DecoderInterfacesTest, AudioDecoderMocking) {
  MockAudioDecoder mock_audio;
  DecodedFrame dummy_frame{{0xAA, 0xBB}, 1000, 1000, 0, 0, 48000, 2};

  EXPECT_CALL(mock_audio, decode(_)).WillOnce(Return(dummy_frame));

  std::vector<uint8_t> packet = {0x01, 0x02};
  auto result = mock_audio.decode(packet);

  EXPECT_EQ(result.sample_rate, 48000);
  EXPECT_EQ(result.channels, 2);
}
