#pragma once

#include <cstdint>
#include <string>

namespace iptv::manifest {

/**
 * @brief Categorization of possible errors encountered during manifest parsing.
 */
enum class ParseErrorCode : uint8_t {
  None = 0,
  EmptyContent,          ///< The provided manifest string is empty.
  InvalidHeader,         ///< Missing #EXTM3U tag on first line.
  InvalidFormat,         ///< General syntax error or unparseable tag.
  UnsupportedVersion,    ///< The HLS version exceeds parser capabilities.
  MissingMandatoryTags,  ///< E.g., segment without preceding #EXTINF.
  InvalidUri             ///< Malformed URI preventing absolute resolution.
};

/**
 * @brief Detailed diagnostic information for manifest parsing failures.
 */
struct ParseError {
  ParseErrorCode code{ParseErrorCode::None};
  uint32_t line_number{0};  ///< 1-based line where failure was encountered.
  std::string message{};      ///< Human-readable diagnostic message.
};

}  // namespace iptv::manifest
