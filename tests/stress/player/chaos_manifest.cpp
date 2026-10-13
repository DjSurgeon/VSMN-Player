#include "chaos_manifest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace iptv::test::chaos {

namespace {

constexpr std::array<std::uint32_t, 8> kVariantBandwidths{
    500'000, 1'000'000, 2'000'000, 4'000'000, 8'000'000, 12'000'000, 20'000'000, 35'000'000};
constexpr std::array<std::array<std::uint32_t, 2>, 8> kVariantResolutions{{{320, 180},
                                                                           {640, 360},
                                                                           {854, 480},
                                                                           {1280, 720},
                                                                           {1920, 1080},
                                                                           {2560, 1440},
                                                                           {3840, 2160},
                                                                           {3840, 2160}}};

}  // namespace

std::size_t variantIndex(int variant, int variant_count) noexcept {
  const int bounded = (variant < 0 || variant >= variant_count) ? 0 : variant;
  return static_cast<std::size_t>(bounded);
}

std::string buildMasterManifest(int variant_count) {
  std::string manifest;
  manifest.reserve(static_cast<std::size_t>(variant_count) * 96 + 16);
  manifest += "#EXTM3U\n";
  for (int v = 0; v < variant_count; ++v) {
    const std::size_t index = static_cast<std::size_t>(v);
    manifest += "#EXT-X-STREAM-INF:BANDWIDTH=";
    manifest += std::to_string(kVariantBandwidths[index]);
    manifest += ",RESOLUTION=";
    manifest += std::to_string(kVariantResolutions[index][0]);
    manifest += 'x';
    manifest += std::to_string(kVariantResolutions[index][1]);
    manifest += ",CODECS=\"avc1.64002a,mp4a.40.2\"\n";
    manifest += 'v';
    manifest += std::to_string(v);
    manifest += ".m3u8\n";
  }
  return manifest;
}

std::string buildMediaManifest(int variant, int segment_count) {
  std::string manifest;
  manifest.reserve(static_cast<std::size_t>(segment_count) * 24 + 64);
  manifest += "#EXTM3U\n";
  manifest += "#EXT-X-TARGETDURATION:4\n";
  manifest += "#EXT-X-MEDIA-SEQUENCE:0\n";
  for (int s = 0; s < segment_count; ++s) {
    manifest += "#EXTINF:4.000000,\n";
    manifest += 'v';
    manifest += std::to_string(variant);
    manifest += "/s";
    manifest += std::to_string(s);
    manifest += ".ts\n";
  }
  manifest += "#EXT-X-ENDLIST\n";
  return manifest;
}

}  // namespace iptv::test::chaos
