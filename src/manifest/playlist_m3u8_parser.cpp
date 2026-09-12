#include "iptv/manifest/playlist_m3u8_parser.hpp"

#include <array>
#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

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
 * @brief Mutable context shared among handlers during parsing.
 */
struct ParseContext {
  Playlist& playlist;
  SegmentBuilder& builder;
  uint32_t line_num{0};
  std::string_view base_url;
};

/**
 * @brief Resolve relative URIs defensively without overengineering.
 */
[[nodiscard]] std::string resolveUri(std::string_view uri, std::string_view base_url) {
  if (uri.starts_with("http://") || uri.starts_with("https://")) {
    return std::string(uri);
  }

  if (uri.starts_with("/")) {
    // Relative to host root
    const auto scheme_end = base_url.find("://");
    if (scheme_end != std::string_view::npos) {
      const auto host_end = base_url.find('/', scheme_end + 3);
      const auto root =
          (host_end != std::string_view::npos) ? base_url.substr(0, host_end) : base_url;
      return std::string(root).append(uri);
    }
  }

  // Relative to current directory of base_url
  const auto last_slash = base_url.rfind('/');
  if (last_slash != std::string_view::npos) {
    return std::string(base_url.substr(0, last_slash + 1)).append(uri);
  }

  return std::string(base_url).append("/").append(uri);
}

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

std::optional<ParseError> commitSegment(std::string_view uri, std::string_view base_url,
                                        uint32_t line_num, SegmentBuilder& builder,
                                        Playlist& playlist) {
  if (!builder.active) {
    return ParseError{ParseErrorCode::MissingMandatoryTags, line_num,
                      "Segment URI without preceding #EXTINF"};
  }

  MediaSegmentRef segment;
  segment.uri = resolveUri(uri, base_url);
  segment.duration = builder.duration;
  segment.sequence_index = playlist.media_sequence + playlist.segments.size();
  segment.is_discontinuity = builder.discontinuity;

  playlist.segments.push_back(std::move(segment));
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
  ctx.builder.discontinuity = true;
  return std::nullopt;
}

std::optional<ParseError> handleExtInf(std::string_view line, ParseContext& ctx) {
  return parseExtInf(line, ctx.line_num, ctx.builder);
}

// Static dispatch table residing in read-only data segment
constexpr std::array<TagDispatchEntry, 5> k_tag_dispatch_table{{
    {"#EXTINF:", &handleExtInf},
    {"#EXT-X-TARGETDURATION:", &handleTargetDuration},
    {"#EXT-X-MEDIA-SEQUENCE:", &handleMediaSequence},
    {"#EXT-X-ENDLIST", &handleEndList},
    {"#EXT-X-DISCONTINUITY", &handleDiscontinuity},
}};

}  // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ParseResult M3u8Parser::parse(std::string_view content, std::string_view base_url) const {
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
  SegmentBuilder builder;

  while (const auto entry = reader.next()) {
    const auto [line, line_num] = *entry;
    ParseContext ctx{playlist, builder, line_num, base_url};

    // 1. Multimedia segment URI line (doesn't start with '#')
    if (!line.starts_with('#')) {
      if (auto err = commitSegment(line, base_url, line_num, builder, playlist)) {
        return *err;
      }
      continue;
    }

    // 2. Dispatch through the static tag table
    for (const auto& dispatch_entry : k_tag_dispatch_table) {
      if (line.starts_with(dispatch_entry.prefix)) {
        if (auto err = dispatch_entry.handler(line, ctx)) {
          return *err;
        }
        break;
      }
    }
  }

  if (!playlist.has_endlist) {
    playlist.type = PlaylistType::MediaLive;
  }

  return playlist;
}

}  // namespace iptv::manifest
