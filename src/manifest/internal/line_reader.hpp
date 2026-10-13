#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace iptv::manifest::internal {

/**
 * @brief Represents a single non-empty line parsed from the manifest.
 */
struct LineEntry {
  std::string_view text;
  uint32_t number{0};
};

/**
 * @brief Zero-allocation line iterator over a manifest's text.
 *
 * Yielding the next non-empty line requires "and" to describe: it advances the
 * cursor, counts the line, and trims the result. The counting lives in next(),
 * the trimming in trim().
 */
class LineReader {
 public:
  /**
   * @brief Constructs a line reader over the given content.
   * @param content The full text to scan.
   */
  explicit LineReader(std::string_view content) noexcept;

  /**
   * @brief Advances to and returns the next non-empty line.
   * @return A LineEntry containing the trimmed text and line number, or nullopt at EOF.
   */
  [[nodiscard]] std::optional<LineEntry> next() noexcept;

 private:
  /**
   * @brief Strips trailing whitespace/carriage returns and leading whitespace.
   */
  [[nodiscard]] static std::string_view trim(std::string_view view) noexcept;

  std::string_view content_;
  size_t cursor_{0};
  uint32_t current_line_{0};
};

}  // namespace iptv::manifest::internal
