#include "internal/playlist_line_parser.hpp"

#include "internal/playlist_tag_handlers.hpp"

namespace iptv::manifest::internal {

namespace {

std::optional<ParseError> commitSegment(std::string_view uri, uint32_t line_num,
                                        SegmentBuilder& builder, Playlist& playlist) {
  if (!builder.active) {
    return ParseError{ParseErrorCode::MissingMandatoryTags, line_num,
                      "Segment URI without preceding #EXTINF"};
  }

  MediaSegmentRef segment;
  segment.uri = uri;
  segment.duration = builder.duration;
  segment.sequence_index = playlist.media_sequence + playlist.segments.size();
  segment.is_discontinuity = builder.discontinuity;

  playlist.segments.push_back(segment);
  resetSegmentBuilder(builder);
  return std::nullopt;
}

std::optional<ParseError> commitVariant(std::string_view uri, uint32_t line_num,
                                        PendingVariant& builder, Playlist& playlist) {
  if (!builder.active) {
    return ParseError{ParseErrorCode::MissingMandatoryTags, line_num,
                      "Variant URI without preceding #EXT-X-STREAM-INF"};
  }

  builder.variant.uri = uri;
  playlist.variants.push_back(builder.variant);
  resetVariantBuilder(builder);
  return std::nullopt;
}

std::optional<ParseError> parseUriLine(std::string_view line, ParseContext& ctx) {
  if (ctx.variant_builder.active) {
    return commitVariant(line, ctx.line_num, ctx.variant_builder, ctx.playlist);
  }
  return commitSegment(line, ctx.line_num, ctx.segment_builder, ctx.playlist);
}

}  // namespace

std::optional<ParseError> commitUriLine(std::string_view uri, uint32_t line_num,
                                        ParseContext& ctx) {
  ctx.line_num = line_num;
  return parseUriLine(uri, ctx);
}

std::optional<ParseError> parseLines(LineReader& reader, Playlist& playlist) {
  SegmentBuilder segment_builder;
  PendingVariant variant_builder;

  while (const auto entry = reader.next()) {
    const auto [line, line_num] = *entry;
    ParseContext ctx{playlist, segment_builder, variant_builder, line_num};

    if (!line.starts_with('#')) {
      if (auto err = commitUriLine(line, line_num, ctx)) {
        return err;
      }
      continue;
    }

    if (auto err = dispatchTag(line, ctx)) {
      return err;
    }
  }
  return std::nullopt;
}

}  // namespace iptv::manifest::internal
