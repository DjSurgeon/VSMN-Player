#include "internal/line_reader.hpp"

namespace iptv::manifest::internal {

LineReader::LineReader(std::string_view content) noexcept : content_(content) {}

std::optional<LineEntry> LineReader::next() noexcept {
  while (cursor_ < content_.size()) {
    const size_t newline_pos = content_.find('\n', cursor_);
    const std::string_view raw = (newline_pos == std::string_view::npos)
                                     ? content_.substr(cursor_)
                                     : content_.substr(cursor_, newline_pos - cursor_);

    cursor_ = (newline_pos == std::string_view::npos) ? content_.size() : newline_pos + 1;
    ++current_line_;

    std::string_view line = trim(raw);
    if (!line.empty()) {
      return LineEntry{line, current_line_};
    }
  }
  return std::nullopt;
}

std::string_view LineReader::trim(std::string_view view) noexcept {
  while (!view.empty() && (view.back() == '\r' || view.back() == ' ' || view.back() == '\t')) {
    view.remove_suffix(1);
  }
  while (!view.empty() && (view.front() == ' ' || view.front() == '\t')) {
    view.remove_prefix(1);
  }
  return view;
}

}  // namespace iptv::manifest::internal
