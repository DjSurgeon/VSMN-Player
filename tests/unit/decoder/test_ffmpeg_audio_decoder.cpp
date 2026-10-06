#include <gtest/gtest.h>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <cstdint>

#include "iptv/decoder/ffmpeg_audio_decoder.hpp"

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

}  // namespace iptv::decoder::test
