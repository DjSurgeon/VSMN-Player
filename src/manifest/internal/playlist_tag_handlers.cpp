#include "internal/playlist_tag_handlers.hpp"

#include <array>
#include <charconv>
#include <system_error>

namespace iptv::manifest::internal {

namespace {

std::optional<ParseError> parseExtInf(std::string_view line, uint32_t line_num,
                                      SegmentBuilder& builder) {
  std::string_view val_str = line.substr(8);
  const size_t comma = val_str.find(',');
  if (comma != std::string_view::npos) {
    val_str = val_str.substr(0, comma);
  }

  double dur_sec = 0.0;
  const auto [ptr, ec] = std::from_chars(val_str.data(), val_str.data() + val_str.size(), dur_sec);
  if (ec != std::errc{}) {
    return ParseError{ParseErrorCode::InvalidFormat, line_num, "Malformed #EXTINF duration"};
  }

  builder.duration = FloatingSeconds(dur_sec);
  builder.active = true;
  return std::nullopt;
}

std::optional<ParseError> handleExtInf(std::string_view line, ParseContext& ctx) {
  return parseExtInf(line, ctx.line_num, ctx.segment_builder);
}

std::optional<ParseError> handleStreamInf(std::string_view line, ParseContext& ctx) {
  ctx.variant_builder.active = true;
  applyStreamInfAttributes(line.substr(18), ctx.variant_builder);
  return std::nullopt;
}

std::optional<ParseError> handleTargetDuration(std::string_view line, ParseContext& ctx) {
  const auto val = line.substr(22);
  uint32_t sec = 0;
  if (auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), sec);
      ec == std::errc{}) {
    ctx.playlist.target_duration = std::chrono::seconds(sec);
  }
  return std::nullopt;
}

std::optional<ParseError> handleMediaSequence(std::string_view line, ParseContext& ctx) {
  const auto val = line.substr(22);
  uint64_t seq = 0;
  if (auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), seq);
      ec == std::errc{}) {
    ctx.playlist.media_sequence = seq;
  }
  return std::nullopt;
}

std::optional<ParseError> handleEndList(std::string_view /*line*/, ParseContext& ctx) {
  ctx.playlist.has_endlist = true;
  ctx.playlist.type = PlaylistType::MediaVOD;
  return std::nullopt;
}

std::optional<ParseError> handleDiscontinuity(std::string_view /*line*/, ParseContext& ctx) {
  ctx.segment_builder.discontinuity = true;
  return std::nullopt;
}

// Static dispatch table residing in the read-only data segment.
constexpr std::array<TagDispatchEntry, 6> k_tag_dispatch_table{{
    {"#EXTINF:", &handleExtInf},
    {"#EXT-X-TARGETDURATION:", &handleTargetDuration},
    {"#EXT-X-MEDIA-SEQUENCE:", &handleMediaSequence},
    {"#EXT-X-ENDLIST", &handleEndList},
    {"#EXT-X-DISCONTINUITY", &handleDiscontinuity},
    {"#EXT-X-STREAM-INF:", &handleStreamInf},
}};

}  // namespace

std::optional<ParseError> dispatchTag(std::string_view line, ParseContext& ctx) {
  for (const auto& entry : k_tag_dispatch_table) {
    if (line.starts_with(entry.prefix)) {
      return entry.handler(line, ctx);
    }
  }
  return std::nullopt;
}

}  // namespace iptv::manifest::internal
