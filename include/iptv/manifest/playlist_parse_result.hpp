#pragma once

#include <utility>
#include <variant>

#include "iptv/manifest/playlist.hpp"
#include "iptv/manifest/playlist_parse_error.hpp"

namespace iptv::manifest {

/**
 * @brief Ergonomic C++20 Result wrapper emulating C++23 std::expected.
 */
class ParseResult {
 public:
  /**
   * @brief Construct from successful Playlist
   * @param playlist The successfully parsed playlist.
   */
  ParseResult(Playlist playlist) noexcept : storage_(std::move(playlist)) {}

  /**
   * @brief Construct from ParseError
   * @param error The diagnostic error.
   */
  ParseResult(ParseError error) noexcept : storage_(std::move(error)) {}

  /**
   * @brief Checks if the result contains a valid playlist.
   * @return true if successful, false if it contains an error.
   */
  [[nodiscard]] bool hasValue() const noexcept {
    return std::holds_alternative<Playlist>(storage_);
  }

  /**
   * @brief Boolean conversion operator, equivalent to hasValue().
   * @return true if successful, false otherwise.
   */
  [[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }

  /**
   * @brief Retrieves the playlist by reference.
   * @return Reference to the playlist.
   */
  [[nodiscard]] Playlist& value() & { return std::get<Playlist>(storage_); }

  /**
   * @brief Retrieves the playlist by const reference.
   * @return Const reference to the playlist.
   */
  [[nodiscard]] const Playlist& value() const& { return std::get<Playlist>(storage_); }

  /**
   * @brief Retrieves the playlist by rvalue reference, allowing move semantics.
   * @return Rvalue reference to the playlist.
   */
  [[nodiscard]] Playlist&& value() && { return std::get<Playlist>(std::move(storage_)); }

  /**
   * @brief Retrieves the diagnostic error.
   * @return Const reference to the ParseError.
   */
  [[nodiscard]] const ParseError& error() const { return std::get<ParseError>(storage_); }

 private:
  std::variant<Playlist, ParseError> storage_;
};

}  // namespace iptv::manifest
