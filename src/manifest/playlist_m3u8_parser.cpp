#include "iptv/manifest/playlist_m3u8_parser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

#include "iptv/manifest/playlist_attribute_scanner.hpp"

namespace iptv::manifest {
namespace {

/**
 * @brief Represents a single line parsed from the manifest.
 */
struct LineEntry {
  std::string_view text;
  uint32_t number{0};
};

/**
 * @brief Zero-allocation line iterator for string views.
 */
class LineReader {
 public:
  /**
   * @brief Constructs a line reader over the given content.
   * @param content The full text to scan.
   */
  explicit LineReader(std::string_view content) noexcept : content_(content) {}

  /**
   * @brief Advances to and returns the next non-empty line.
   * @return A LineEntry containing the trimmed text and line number, or nullopt if EOF.
   */
  std::optional<LineEntry> next() noexcept {
    while (cursor_ < content_.size()) {
      const size_t newline_pos = content_.find('\n', cursor_);
      std::string_view raw = (newline_pos == std::string_view::npos)
                                 ? content_.substr(cursor_)
                                 : content_.substr(cursor_, newline_pos - cursor_);

      cursor_ = (newline_pos == std::string_view::npos) ? content_.size() : newline_pos + 1;
      ++current_line_;

      auto line = trim(raw);
      if (!line.empty()) {
        return LineEntry{line, current_line_};
      }
    }
    return std::nullopt;
  }

 private:
  /**
   * @brief Trim whitespace and carriage returns from a string view.
   */
  static std::string_view trim(std::string_view view) noexcept {
    while (!view.empty() && (view.back() == '\r' || view.back() == ' ' || view.back() == '\t')) {
      view.remove_suffix(1);
    }
    while (!view.empty() && (view.front() == ' ' || view.front() == '\t')) {
      view.remove_prefix(1);
    }
    return view;
  }

  std::string_view content_;
  size_t cursor_{0};
  uint32_t current_line_{0};
};

/**
 * @brief Ephemeral state for building segments across multiple lines.
 */
struct SegmentBuilder {
  bool active{false};
  FloatingSeconds duration{0.0};
  bool discontinuity{false};

  /**
   * @brief Resets the builder state after committing a segment.
   */
  void reset() noexcept {
    active = false;
    discontinuity = false;
  }
};

/**
 * @brief Ephemeral state for building variants across multiple lines.
 */
struct PendingVariant {
  bool active{false};
  VariantStreamRef variant{};

  /**
   * @brief Resets the builder state after committing a variant.
   */
  void reset() noexcept {
    active = false;
    variant = VariantStreamRef{};
  }
};

/**
 * @brief Mutable context shared among handlers during parsing.
 */
struct ParseContext {
  Playlist& playlist;
  SegmentBuilder& segment_builder;
  PendingVariant& variant_builder;
  uint32_t line_num{0};
};

// resolveUri is removed. It's the network layer's responsibility to resolve relative URLs.

std::optional<ParseError> parseExtInf(std::string_view line, uint32_t line_num,
                                      SegmentBuilder& builder) {
  auto val_str = line.substr(8);
  const auto comma = val_str.find(',');
  if (comma != std::string_view::npos) {
    val_str = val_str.substr(0, comma);
  }

  double dur_sec = 0.0;
  auto [ptr, ec] = std::from_chars(val_str.data(), val_str.data() + val_str.size(), dur_sec);
  if (ec != std::errc{}) {
    return ParseError{ParseErrorCode::InvalidFormat, line_num, "Malformed #EXTINF duration"};
  }

  builder.duration = FloatingSeconds(dur_sec);
  builder.active = true;
  return std::nullopt;
}

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
  builder.reset();
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
  builder.reset();
  return std::nullopt;
}

void parseTargetDuration(std::string_view line, Playlist& playlist) noexcept {
  const auto val = line.substr(22);
  uint32_t sec = 0;
  if (auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), sec); ec == std::errc{}) {
    playlist.target_duration = std::chrono::seconds(sec);
  }
}

void parseMediaSequence(std::string_view line, Playlist& playlist) noexcept {
  const auto val = line.substr(22);
  uint64_t seq = 0;
  if (auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), seq); ec == std::errc{}) {
    playlist.media_sequence = seq;
  }
}

/**
 * @brief Semantic handler for parsing BANDWIDTH attributes.
 */
void parseBandwidth(std::string_view val, PendingVariant& builder) noexcept {
  uint32_t bandwidth = 0;
  if (auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), bandwidth);
      ec == std::errc{}) {
    builder.variant.bandwidth = bandwidth;
  }
}

/**
 * @brief Semantic handler for parsing RESOLUTION attributes.
 */
void parseResolution(std::string_view val, PendingVariant& builder) noexcept {
  const size_t x_pos = val.find('x');
  if (x_pos != std::string_view::npos) {
    auto w_str = val.substr(0, x_pos);
    auto h_str = val.substr(x_pos + 1);
    uint32_t width = 0;
    uint32_t height = 0;
    auto [pw, ecw] = std::from_chars(w_str.data(), w_str.data() + w_str.size(), width);
    auto [ph, ech] = std::from_chars(h_str.data(), h_str.data() + h_str.size(), height);
    if (ecw == std::errc{} && ech == std::errc{}) {
      builder.variant.resolution = {width, height};
    }
  }
}

/**
 * @brief Semantic handler for parsing FRAME-RATE attributes.
 */
void parseFrameRate(std::string_view val, PendingVariant& builder) noexcept {
  double fps = 0.0;
  if (auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), fps); ec == std::errc{}) {
    builder.variant.frame_rate = fps;
  }
}

std::optional<ParseError> parseStreamInf(std::string_view line, uint32_t /*line_num*/,
                                         PendingVariant& builder) {
  auto attrs_str = line.substr(18);
  builder.active = true;

  auto scanner = AttributeScanner{attrs_str};
  while (const auto attr = scanner.next()) {
    // NOLINTNEXTLINE(bugprone-branch-clone)
    if (attr->key == "BANDWIDTH") {
      parseBandwidth(attr->value, builder);
    } else if (attr->key == "RESOLUTION") {
      parseResolution(attr->value, builder);
    } else if (attr->key == "FRAME-RATE") {
      parseFrameRate(attr->value, builder);
    } else if (attr->key == "CODECS") {
      builder.variant.codecs = attr->value;
    }
  }

  return std::nullopt;
}

// Canonical signature for an HLS tag handler
using TagHandler = std::optional<ParseError> (*)(std::string_view line, ParseContext& ctx);

struct TagDispatchEntry {
  std::string_view prefix;
  TagHandler handler;
};

std::optional<ParseError> handleTargetDuration(std::string_view line, ParseContext& ctx) {
  parseTargetDuration(line, ctx.playlist);
  return std::nullopt;
}

std::optional<ParseError> handleMediaSequence(std::string_view line, ParseContext& ctx) {
  parseMediaSequence(line, ctx.playlist);
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

std::optional<ParseError> handleExtInf(std::string_view line, ParseContext& ctx) {
  return parseExtInf(line, ctx.line_num, ctx.segment_builder);
}

std::optional<ParseError> handleStreamInf(std::string_view line, ParseContext& ctx) {
  return parseStreamInf(line, ctx.line_num, ctx.variant_builder);
}

// Static dispatch table residing in read-only data segment
constexpr std::array<TagDispatchEntry, 6> k_tag_dispatch_table{{
    {"#EXTINF:", &handleExtInf},
    {"#EXT-X-TARGETDURATION:", &handleTargetDuration},
    {"#EXT-X-MEDIA-SEQUENCE:", &handleMediaSequence},
    {"#EXT-X-ENDLIST", &handleEndList},
    {"#EXT-X-DISCONTINUITY", &handleDiscontinuity},
    {"#EXT-X-STREAM-INF:", &handleStreamInf},
}};

void preallocatePlaylist(std::string_view content, Playlist& playlist) {
  // Pre-allocation heuristic to achieve exact 1 allocation per playlist
  const bool is_master = content.find("#EXT-X-STREAM-INF:") != std::string_view::npos;
  const size_t estimated_elements = content.size() / 48;

  if (is_master) {
    playlist.variants.reserve(estimated_elements);
  } else {
    playlist.segments.reserve(estimated_elements);
  }
}

std::optional<ParseError> parseLines(LineReader& reader, Playlist& playlist) {
  SegmentBuilder segment_builder;
  PendingVariant variant_builder;

  while (const auto entry = reader.next()) {
    const auto [line, line_num] = *entry;
    ParseContext ctx{playlist, segment_builder, variant_builder, line_num};

    if (!line.starts_with('#')) {
      if (variant_builder.active) {
        if (auto err = commitVariant(line, line_num, variant_builder, playlist)) {
          return err;
        }
      } else {
        if (auto err = commitSegment(line, line_num, segment_builder, playlist)) {
          return err;
        }
      }
      continue;
    }

    // 2. Dispatch through the static tag table
    for (const auto& dispatch_entry : k_tag_dispatch_table) {
      if (line.starts_with(dispatch_entry.prefix)) {
        if (auto err = dispatch_entry.handler(line, ctx)) {
          return err;
        }
        break;
      }
    }
  }
  return std::nullopt;
}

void finalizePlaylist(Playlist& playlist) {
  if (!playlist.variants.empty()) {
    playlist.type = PlaylistType::Master;

    // Sort variants ascending by bandwidth (Fast Start strategy standard)
    std::sort(playlist.variants.begin(), playlist.variants.end(),
              [](const auto& left, const auto& right) { return left.bandwidth < right.bandwidth; });
  } else if (!playlist.has_endlist) {
    playlist.type = PlaylistType::MediaLive;
  }
}

}  // namespace

namespace M3u8Parser {

ParseResult parse(const ParseOptions& options) {
  std::string_view content = options.content;
  uint32_t first_line_num = 0;

  if (content.starts_with("\xEF\xBB\xBF")) {
    content.remove_prefix(3);
  }

  if (content.empty()) {
    return ParseError{ParseErrorCode::EmptyContent, 0, "Manifest content is empty"};
  }

  LineReader reader(content);
  const auto first_line = reader.next();
  if (!first_line || first_line->text != "#EXTM3U") {
    return ParseError{ParseErrorCode::InvalidHeader, first_line ? first_line->number : 0,
                      "Missing #EXTM3U tag"};
  }

  Playlist playlist;
  preallocatePlaylist(options.content, playlist);

  if (auto err = parseLines(reader, playlist)) {
    return *err;
  }

  finalizePlaylist(playlist);

  return playlist;
}
}  // namespace M3u8Parser

}  // namespace iptv::manifest
