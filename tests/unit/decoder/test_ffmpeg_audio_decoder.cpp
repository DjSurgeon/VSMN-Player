#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "iptv/decoder/ffmpeg_audio_decoder.hpp"
#include "aac_source.hpp"

using iptv::decoder::testing::encodeAacSineWave;

namespace iptv::decoder::test {

TEST(FFmpegAudioDecoderTest, InstantiationWithValidCodec) {
  EXPECT_NO_THROW({ FFmpegAudioDecoder decoder("aac"); });
  EXPECT_NO_THROW({ FFmpegAudioDecoder decoder("mp3"); });
}

TEST(FFmpegAudioDecoderTest, InstantiationWithInvalidCodecThrows) {
  EXPECT_THROW({ FFmpegAudioDecoder decoder("invalid_codec_name"); }, std::runtime_error);
}

TEST(FFmpegAudioDecoderTest, IsMoveConstructibleAndMoveAssignable) {
  EXPECT_TRUE(std::is_move_constructible_v<FFmpegAudioDecoder>);
  EXPECT_TRUE(std::is_move_assignable_v<FFmpegAudioDecoder>);
  EXPECT_FALSE(std::is_copy_constructible_v<FFmpegAudioDecoder>);
  EXPECT_FALSE(std::is_copy_assignable_v<FFmpegAudioDecoder>);
}

TEST(FFmpegAudioDecoderTest, UseAfterMoveThrowsLogicError) {
  FFmpegAudioDecoder original("aac");
  FFmpegAudioDecoder moved(std::move(original));

  std::vector<uint8_t> dummy_data = {0x00, 0x01, 0x02};
  
  // Calling decode on moved-from object should throw
  EXPECT_THROW({ original.decode(dummy_data); }, std::logic_error);
  
  // Calling flush on moved-from object should throw
  EXPECT_THROW({ original.flush(); }, std::logic_error);
  
  // Calling getCodecInfo on moved-from object should throw
  EXPECT_THROW({ original.getCodecInfo(); }, std::logic_error);
}

TEST(FFmpegAudioDecoderTest, GetCodecInfoReturnsCorrectName) {
  FFmpegAudioDecoder aac_decoder("aac");
  EXPECT_EQ(aac_decoder.getCodecInfo().name, "aac");

  FFmpegAudioDecoder mp3_decoder("mp3");
  EXPECT_EQ(mp3_decoder.getCodecInfo().name, "mp3"); // FFmpeg might resolve "mp3" to "mp3float" or "mp3"
}

TEST(FFmpegAudioDecoderTest, EmptyDecodeReturnsEmptyFrames) {
  FFmpegAudioDecoder decoder("aac");
  std::vector<uint8_t> empty_data;
  auto frames = decoder.decode(empty_data);
  EXPECT_TRUE(frames.empty());
}

TEST(FFmpegAudioDecoderTest, AacDecodeAndResample44100To48000) {
  constexpr int kSourceRate = 44100;
  constexpr int kChannels = 2;
  constexpr int kDurationMs = 200;

  auto packets = encodeAacSineWave(kSourceRate, kChannels, kDurationMs);
  ASSERT_FALSE(packets.empty());

  FFmpegAudioDecoder decoder("aac");
  std::vector<DecodedFrame> all_frames;

  for (const auto& packet : packets) {
    auto frames = decoder.decode(packet);
    all_frames.insert(all_frames.end(), std::make_move_iterator(frames.begin()),
                      std::make_move_iterator(frames.end()));
  }

  auto flushed = decoder.flush();
  all_frames.insert(all_frames.end(), std::make_move_iterator(flushed.begin()),
                    std::make_move_iterator(flushed.end()));

  ASSERT_FALSE(all_frames.empty());
  for (const auto& frame : all_frames) {
    EXPECT_EQ(frame.media_type, MediaType::Audio);
    EXPECT_EQ(frame.sample_rate, 48000);
    EXPECT_EQ(frame.channels, 2);
    EXPECT_EQ(frame.audio_format, AudioFormat::PCM_S16_48KHZ);
    EXPECT_FALSE(frame.data.empty());
  }
}

TEST(FFmpegAudioDecoderTest, PartialPacketDropsDoNotThrow) {
  constexpr int kSourceRate = 44100;
  constexpr int kChannels = 2;
  constexpr int kDurationMs = 200;

  auto packets = encodeAacSineWave(kSourceRate, kChannels, kDurationMs);
  ASSERT_GT(packets.size(), 2u);

  FFmpegAudioDecoder decoder("aac");

  // Drop every other packet to simulate partial loss.
  for (size_t i = 0; i < packets.size(); i += 2) {
    EXPECT_NO_THROW({ decoder.decode(packets[i]); });
  }

  EXPECT_NO_THROW({ decoder.flush(); });
}

}  // namespace iptv::decoder::test
