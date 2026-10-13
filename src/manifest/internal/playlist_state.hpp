#pragma once

#include <cstdint>
#include <string_view>

#include "iptv/manifest/playlist.hpp"

namespace iptv::manifest::internal {

/**
 * @brief Ephemeral state for building a segment across the lines that describe it.
 */
struct SegmentBuilder {
  bool active{false};
  FloatingSeconds duration{0.0};
  bool discontinuity{false};
};

/**
 * @brief Ephemeral state for building a variant across the lines that describe it.
 */
struct PendingVariant {
  bool active{false};
  VariantStreamRef variant{};
};

/**
 * @brief Mutable context shared among tag handlers during parsing.
 */
struct ParseContext {
  Playlist& playlist;
  SegmentBuilder& segment_builder;
  PendingVariant& variant_builder;
  uint32_t line_num{0};
};

/**
 * @brief Resets the segment builder after a segment has been committed.
 */
void resetSegmentBuilder(SegmentBuilder& builder) noexcept;

/**
 * @brief Resets the variant builder after a variant has been committed.
 */
void resetVariantBuilder(PendingVariant& builder) noexcept;

/**
 * @brief Applies the attribute list of #EXT-X-STREAM-INF to the pending variant.
 * @param attrs Attribute list without the tag name, e.g. "BANDWIDTH=800,CODECS=\"avc1\"".
 * @param builder Builder whose variant receives the decoded attributes.
 */
void applyStreamInfAttributes(std::string_view attrs, PendingVariant& builder) noexcept;

}  // namespace iptv::manifest::internal
