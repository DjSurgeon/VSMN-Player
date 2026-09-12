#include "iptv/manifest/playlist_attribute_scanner.hpp"

namespace iptv::manifest {

std::optional<AttributePair> AttributeScanner::next() noexcept {
  if (cursor_ >= attrs_.size()) {
    return std::nullopt;
  }

  auto key = readKey();
  if (key.empty()) {
    return std::nullopt;  // Malformed, abort scanning
  }

  auto value = stripQuotes(readValue());
  skipComma();

  return AttributePair{key, value};
}

std::string_view AttributeScanner::readKey() noexcept {
  const size_t eq_pos = attrs_.find('=', cursor_);
  if (eq_pos == std::string_view::npos) {
    cursor_ = attrs_.size();
    return {};
  }

  std::string_view key = attrs_.substr(cursor_, eq_pos - cursor_);
  cursor_ = eq_pos + 1;
  return key;
}

std::string_view AttributeScanner::readValue() noexcept {
  bool in_quotes = false;
  size_t val_start = cursor_;

  while (cursor_ < attrs_.size()) {
    if (attrs_[cursor_] == '"') {
      in_quotes = !in_quotes;
    } else if (attrs_[cursor_] == ',' && !in_quotes) {
      break;
    }
    cursor_++;
  }

  return attrs_.substr(val_start, cursor_ - val_start);
}

std::string_view AttributeScanner::stripQuotes(std::string_view val) const noexcept {
  if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
    val.remove_prefix(1);
    val.remove_suffix(1);
  }
  return val;
}

void AttributeScanner::skipComma() noexcept {
  if (cursor_ < attrs_.size() && attrs_[cursor_] == ',') {
    cursor_++;
  }
}

}  // namespace iptv::manifest
