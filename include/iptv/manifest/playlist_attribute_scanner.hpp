#pragma once


#include <optional>
#include <string_view>

namespace iptv::manifest {

/**
 * @brief Represents a single key-value attribute pair from an HLS tag.
 */
struct AttributePair {
  std::string_view key;
  std::string_view value;
};

/**
 * @brief Lexical scanner for parsing HLS attribute lists (e.g. BANDWIDTH=800,CODECS="...").
 *
 * Safely handles quoted strings containing commas.
 */
class AttributeScanner {
 public:
  /**
   * @brief Constructs a scanner over the attribute list string.
   */
  explicit AttributeScanner(std::string_view attrs) noexcept : attrs_(attrs) {}

  /**
   * @brief Advances to and returns the next attribute pair.
   */
  std::optional<AttributePair> next() noexcept;

 protected:
  /**
   * @brief Extracts the attribute key up to the equals sign.
   */
  std::string_view readKey() noexcept;

  /**
   * @brief Lexes the attribute value, respecting quotes.
   */
  std::string_view readValue() noexcept;

  /**
   * @brief Removes surrounding double quotes if present.
   */
  [[nodiscard]] static std::string_view stripQuotes(std::string_view val) noexcept;

  /**
   * @brief Advances the cursor past the comma separator if present.
   */
  void skipComma() noexcept;

  // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
  std::string_view attrs_;
  size_t cursor_{0};
  // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)
};

}  // namespace iptv::manifest
