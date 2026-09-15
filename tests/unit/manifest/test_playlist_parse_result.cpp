#include <gtest/gtest.h>

#include "iptv/manifest/playlist_parse_result.hpp"

using namespace iptv::manifest;

TEST(PlaylistParseResultTest, ConstructsWithSuccess) {
  Playlist pl;
  pl.target_duration = std::chrono::seconds(10);
  pl.has_endlist = true;

  ParseResult result(std::move(pl));

  EXPECT_TRUE(result.hasValue());
  EXPECT_TRUE(static_cast<bool>(result));
  EXPECT_EQ(result.value().target_duration.count(), 10);
  EXPECT_TRUE(result.value().has_endlist);
}

TEST(PlaylistParseResultTest, ConstructsWithError) {
  ParseError err{ParseErrorCode::InvalidFormat, 42, "Bad syntax"};

  ParseResult result(std::move(err));

  EXPECT_FALSE(result.hasValue());
  EXPECT_FALSE(static_cast<bool>(result));
  EXPECT_EQ(result.error().code, ParseErrorCode::InvalidFormat);
  EXPECT_EQ(result.error().line_number, 42);
  EXPECT_EQ(result.error().message, "Bad syntax");
}

TEST(PlaylistParseResultTest, MoveSemanticsArePreserved) {
  Playlist pl;
  pl.segments.push_back(MediaSegmentRef{"http://test.ts", FloatingSeconds(2.0), 1, false});

  ParseResult result(std::move(pl));
  EXPECT_TRUE(result.hasValue());

  // Use std::move to invoke the && value() overload
  Playlist moved_pl = std::move(result).value();

  EXPECT_EQ(moved_pl.segments.size(), 1);
  EXPECT_EQ(moved_pl.segments[0].uri, "http://test.ts");
}
