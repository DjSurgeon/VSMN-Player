#pragma once

#include <cstddef>
#include <string>

namespace iptv::test::chaos {

/**
 * @brief Renders the master playlist listing every variant.
 * @param variant_count Number of variants to advertise, clamped to the table.
 */
[[nodiscard]] std::string buildMasterManifest(int variant_count);

/**
 * @brief Renders the media playlist of one variant.
 * @param variant The variant ordinal, which appears in each segment URI.
 * @param segment_count Number of segments to advertise.
 */
[[nodiscard]] std::string buildMediaManifest(int variant, int segment_count);

/**
 * @brief Index into the variant bandwidth/resolution tables, clamped to range.
 */
[[nodiscard]] std::size_t variantIndex(int variant, int variant_count) noexcept;

}  // namespace iptv::test::chaos
