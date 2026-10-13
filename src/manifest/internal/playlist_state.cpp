#include "internal/playlist_state.hpp"

#include <charconv>
#include <system_error>

#include "iptv/manifest/playlist_attribute_scanner.hpp"

namespace iptv::manifest::internal {

namespace {

void parseBandwidth(std::string_view val, PendingVariant& builder) noexcept {
  uint32_t bandwidth = 0;
  if (auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), bandwidth);
      ec == std::errc{}) {
    builder.variant.bandwidth = bandwidth;
  }
}

void parseResolution(std::string_view val, PendingVariant& builder) noexcept {
  const size_t x_pos = val.find('x');
  if (x_pos == std::string_view::npos) {
    return;
  }

  const std::string_view w_str = val.substr(0, x_pos);
  const std::string_view h_str = val.substr(x_pos + 1);
  uint32_t width = 0;
  uint32_t height = 0;
  const auto [pw, ecw] = std::from_chars(w_str.data(), w_str.data() + w_str.size(), width);
  const auto [ph, ech] = std::from_chars(h_str.data(), h_str.data() + h_str.size(), height);
  if (ecw == std::errc{} && ech == std::errc{}) {
    builder.variant.resolution = {width, height};
  }
}

void parseFrameRate(std::string_view val, PendingVariant& builder) noexcept {
  double fps = 0.0;
  if (auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), fps);
      ec == std::errc{}) {
    builder.variant.frame_rate = fps;
  }
}

}  // namespace

void resetSegmentBuilder(SegmentBuilder& builder) noexcept {
  builder.active = false;
  builder.discontinuity = false;
}

void resetVariantBuilder(PendingVariant& builder) noexcept {
  builder.active = false;
  builder.variant = VariantStreamRef{};
}

void applyStreamInfAttributes(std::string_view attrs, PendingVariant& builder) noexcept {
  auto scanner = AttributeScanner{attrs};
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
}

}  // namespace iptv::manifest::internal
