#include <gtest/gtest.h>

#include "iptv/manifest/playlist_m3u8_parser.hpp"

using namespace iptv::manifest;

class M3u8ParserTest : public ::testing::Test {
 protected:
  M3u8Parser parser;
};

TEST_F(M3u8ParserTest, ParsesEmptyContent) {
  auto result = parser.parse("", "http://example.com/");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, ParseErrorCode::EmptyContent);
}

TEST_F(M3u8ParserTest, ParsesInvalidHeader) {
  auto result = parser.parse("#EXT-X-VERSION:3\n", "http://example.com/");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, ParseErrorCode::InvalidHeader);
}

TEST_F(M3u8ParserTest, ParsesValidVODPlaylist) {
  std::string_view content =
      "#EXTM3U\n"
      "#EXT-X-TARGETDURATION:10\n"
      "#EXT-X-MEDIA-SEQUENCE:1\n"
      "#EXTINF:10.0,\n"
      "seg1.ts\n"
      "#EXTINF:10.0,\n"
      "seg2.ts\n"
      "#EXT-X-ENDLIST\n";

  auto result = parser.parse(content, "http://example.com/stream/");
  ASSERT_TRUE(result.hasValue());
  auto playlist = std::move(result).value();

  EXPECT_EQ(playlist.type, PlaylistType::MediaVOD);
  EXPECT_TRUE(playlist.has_endlist);
  EXPECT_EQ(playlist.target_duration.count(), 10);
  EXPECT_EQ(playlist.media_sequence, 1);
  ASSERT_EQ(playlist.segments.size(), 2);
  EXPECT_EQ(playlist.segments[0].uri, "http://example.com/stream/seg1.ts");
  EXPECT_EQ(playlist.segments[0].duration.count(), 10.0);
  EXPECT_EQ(playlist.segments[0].sequence_index, 1);
  EXPECT_EQ(playlist.segments[1].uri, "http://example.com/stream/seg2.ts");
  EXPECT_EQ(playlist.segments[1].sequence_index, 2);
}

TEST_F(M3u8ParserTest, ResolvesUrlsCorrectly) {
  std::string_view content =
      "#EXTM3U\n"
      "#EXTINF:10.0,\n"
      "seg1.ts\n"
      "#EXTINF:10.0,\n"
      "/root/seg2.ts\n"
      "#EXTINF:10.0,\n"
      "https://other.com/seg3.ts\n";

  auto result = parser.parse(content, "http://example.com/path/playlist.m3u8");
  ASSERT_TRUE(result.hasValue());
  auto playlist = std::move(result).value();

  ASSERT_EQ(playlist.segments.size(), 3);
  EXPECT_EQ(playlist.segments[0].uri, "http://example.com/path/seg1.ts");
  EXPECT_EQ(playlist.segments[1].uri, "http://example.com/root/seg2.ts");
  EXPECT_EQ(playlist.segments[2].uri, "https://other.com/seg3.ts");
}

TEST_F(M3u8ParserTest, RejectsOrphanSegmentUri) {
  std::string_view content =
      "#EXTM3U\n"
      "#EXT-X-TARGETDURATION:10\n"
      "seg1.ts\n";

  auto result = parser.parse(content, "http://example.com/");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, ParseErrorCode::MissingMandatoryTags);
  EXPECT_EQ(result.error().line_number, 3);
}
