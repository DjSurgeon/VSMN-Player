#include "internal/url_resolver.hpp"

namespace iptv::player::internal {

namespace {

/**
 * @brief Reports whether a URI already carries its own scheme.
 */
[[nodiscard]] bool isAbsolute(std::string_view uri) noexcept {
  return uri.starts_with("http://") || uri.starts_with("https://");
}

/**
 * @brief Resolves a root-relative URI against the origin of the base URL.
 *
 * The origin ends at the first slash after "://", so everything from that
 * slash on is the path and is replaced rather than appended to.
 */
[[nodiscard]] std::string resolveRootRelative(std::string_view base_url, std::string_view uri) {
  const size_t host_end = base_url.find('/', base_url.find("://") + 3);
  if (host_end == std::string_view::npos) {
    return std::string(base_url) + std::string(uri);
  }
  return std::string(base_url.substr(0, host_end)) + std::string(uri);
}

/**
 * @brief Resolves a path-relative URI against the directory of the base URL.
 */
[[nodiscard]] std::string resolvePathRelative(std::string_view base_url, std::string_view uri) {
  const size_t last_slash = base_url.find_last_of('/');
  return std::string(base_url.substr(0, last_slash + 1)) + std::string(uri);
}

}  // namespace

std::string resolveUrl(std::string_view base_url, std::string_view uri) {
  if (isAbsolute(uri)) {
    return std::string(uri);
  }

  const size_t last_slash = base_url.find_last_of('/');
  if (last_slash == std::string_view::npos) {
    return std::string(uri);
  }

  if (uri.starts_with('/')) {
    return resolveRootRelative(base_url, uri);
  }
  return resolvePathRelative(base_url, uri);
}

}  // namespace iptv::player::internal
