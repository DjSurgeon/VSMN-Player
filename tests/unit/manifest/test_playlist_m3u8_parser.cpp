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

TEST_F(M3u8ParserTest, ParsesLivePlaylist) {
  std::string_view content =
      "#EXTM3U\n"
      "#EXT-X-TARGETDURATION:10\n"
      "#EXT-X-MEDIA-SEQUENCE:100\n"
      "#EXTINF:10.0,\n"
      "seg100.ts\n"
      "#EXTINF:10.0,\n"
      "seg101.ts\n";

  auto result = parser.parse(content, "http://example.com/stream/");
  ASSERT_TRUE(result.hasValue());
  auto playlist = std::move(result).value();

  EXPECT_EQ(playlist.type, PlaylistType::MediaLive);
  EXPECT_FALSE(playlist.has_endlist);
  EXPECT_EQ(playlist.media_sequence, 100);
  ASSERT_EQ(playlist.segments.size(), 2);
}

TEST_F(M3u8ParserTest, HandlesWindowsLineEndings) {
  std::string_view content =
      "#EXTM3U\r\n"
      "#EXTINF:10.0,\r\n"
      "seg1.ts\r\n"
      "#EXTINF:10.0,\r\n"
      "seg2.ts\r\n";

  auto result = parser.parse(content, "http://example.com/");
  ASSERT_TRUE(result.hasValue());
  auto playlist = std::move(result).value();
  ASSERT_EQ(playlist.segments.size(), 2);
  // Ensure '\r' was trimmed
  EXPECT_EQ(playlist.segments[0].uri, "http://example.com/seg1.ts");
  EXPECT_EQ(playlist.segments[1].uri, "http://example.com/seg2.ts");
}

TEST_F(M3u8ParserTest, ResolvesUrlsCorrectly) {
  // 7 distinct path types
  std::string_view content =
      "#EXTM3U\n"
      "#EXTINF:10.0,\n"
      "seg1.ts\n"
      "#EXTINF:10.0,\n"
      "/root/seg2.ts\n"
      "#EXTINF:10.0,\n"
      "https://other.com/seg3.ts\n"
      "#EXTINF:10.0,\n"
      "http://other.com/seg4.ts\n"
      "#EXTINF:10.0,\n"
      "chunks/seg5.ts\n"
      "#EXTINF:10.0,\n"
      "seg6.ts?token=123\n"
      "#EXTINF:10.0,\n"
      "dir.with.dots/seg7.ts\n";

  auto result = parser.parse(content, "http://example.com/path/playlist.m3u8");
  ASSERT_TRUE(result.hasValue());
  auto playlist = std::move(result).value();

  ASSERT_EQ(playlist.segments.size(), 7);
  EXPECT_EQ(playlist.segments[0].uri, "http://example.com/path/seg1.ts");
  EXPECT_EQ(playlist.segments[1].uri, "http://example.com/root/seg2.ts");
  EXPECT_EQ(playlist.segments[2].uri, "https://other.com/seg3.ts");
  EXPECT_EQ(playlist.segments[3].uri, "http://other.com/seg4.ts");
  EXPECT_EQ(playlist.segments[4].uri, "http://example.com/path/chunks/seg5.ts");
  EXPECT_EQ(playlist.segments[5].uri, "http://example.com/path/seg6.ts?token=123");
  EXPECT_EQ(playlist.segments[6].uri, "http://example.com/path/dir.with.dots/seg7.ts");
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

TEST_F(M3u8ParserTest, RejectsMalformedExtInf) {
  std::string_view content =
      "#EXTM3U\n"
      "#EXTINF:invalid_number,\n"
      "seg1.ts\n";

  auto result = parser.parse(content, "http://example.com/");
  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, ParseErrorCode::InvalidFormat);
  EXPECT_EQ(result.error().line_number, 2);
}

TEST_F(M3u8ParserTest, HandlesDiscontinuity) {
  std::string_view content =
      "#EXTM3U\n"
      "#EXTINF:10.0,\n"
      "seg1.ts\n"
      "#EXT-X-DISCONTINUITY\n"
      "#EXTINF:10.0,\n"
      "seg2.ts\n"
      "#EXTINF:10.0,\n"
      "seg3.ts\n";

  auto result = parser.parse(content, "http://example.com/");
  ASSERT_TRUE(result.hasValue());
  auto playlist = std::move(result).value();
  ASSERT_EQ(playlist.segments.size(), 3);

  EXPECT_FALSE(playlist.segments[0].is_discontinuity);
  EXPECT_TRUE(playlist.segments[1].is_discontinuity);   // seg2 follows DISCONTINUITY
  EXPECT_FALSE(playlist.segments[2].is_discontinuity);  // seg3 should be false
}

TEST_F(M3u8ParserTest, ParsesMasterPlaylist) {
  std::string_view content =
      "#EXTM3U\n"
      // Variant 1: standard mid-tier
      "#EXT-X-STREAM-INF:BANDWIDTH=3000000,RESOLUTION=1280x720,FRAME-RATE=60.0,CODECS=\"avc1."
      "4d401f,mp4a.40.2\"\n"
      "720p.m3u8\n"
      // Variant 2: low-tier (should be sorted first)
      "#EXT-X-STREAM-INF:BANDWIDTH=800000,RESOLUTION=640x360\n"
      "360p.m3u8\n"
      // Variant 3: top-tier
      "#EXT-X-STREAM-INF:BANDWIDTH=6000000,RESOLUTION=1920x1080,CODECS=\"avc1.64002a\"\n"
      "1080p.m3u8\n"
      // Variant 4: audio-only or very low bandwidth, missing resolution entirely
      "#EXT-X-STREAM-INF:BANDWIDTH=128000,CODECS=\"mp4a.40.2\"\n"
      "audio_only.m3u8\n"
      // Variant 5: weird edge case - unrecognized custom attribute, weird framerate, malformed
      // resolution (ignored)
      "#EXT-X-STREAM-INF:BANDWIDTH=4000000,RESOLUTION=weird_res,FRAME-RATE=30.0,X-CUSTOM-CDN="
      "\"yes\"\n"
      "custom.m3u8\n";

  auto result = parser.parse(content, "http://example.com/");
  ASSERT_TRUE(result.hasValue());
  auto playlist = std::move(result).value();

  EXPECT_EQ(playlist.type, PlaylistType::Master);
  ASSERT_EQ(playlist.variants.size(), 5);

  // Assert variants are sorted ascending by bandwidth (Fast Start strategy)

  // 1: 128 Kbps (Audio only)
  EXPECT_EQ(playlist.variants[0].bandwidth, 128000);
  EXPECT_EQ(playlist.variants[0].uri, "http://example.com/audio_only.m3u8");
  EXPECT_FALSE(playlist.variants[0].resolution.isValid());
  EXPECT_EQ(playlist.variants[0].codecs, "mp4a.40.2");

  // 2: 800 Kbps (360p)
  EXPECT_EQ(playlist.variants[1].bandwidth, 800000);
  EXPECT_EQ(playlist.variants[1].uri, "http://example.com/360p.m3u8");
  EXPECT_EQ(playlist.variants[1].resolution.width, 640);
  EXPECT_EQ(playlist.variants[1].resolution.height, 360);

  // 3: 3 Mbps (720p)
  EXPECT_EQ(playlist.variants[2].bandwidth, 3000000);
  EXPECT_EQ(playlist.variants[2].uri, "http://example.com/720p.m3u8");
  EXPECT_EQ(playlist.variants[2].frame_rate, 60.0);
  EXPECT_EQ(playlist.variants[2].codecs, "avc1.4d401f,mp4a.40.2");

  // 4: 4 Mbps (weird custom)
  EXPECT_EQ(playlist.variants[3].bandwidth, 4000000);
  EXPECT_EQ(playlist.variants[3].uri, "http://example.com/custom.m3u8");
  EXPECT_EQ(playlist.variants[3].frame_rate, 30.0);
  EXPECT_FALSE(
      playlist.variants[3].resolution.isValid());  // 'weird_res' parse should fail silently

  // 5: 6 Mbps (1080p)
  EXPECT_EQ(playlist.variants[4].bandwidth, 6000000);
  EXPECT_EQ(playlist.variants[4].uri, "http://example.com/1080p.m3u8");
  EXPECT_EQ(playlist.variants[4].resolution.width, 1920);
  EXPECT_EQ(playlist.variants[4].codecs, "avc1.64002a");
}
