#include "internal/ffmpeg_frame_extractor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "iptv/decoder/decoded_frame.hpp"

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace iptv::decoder::internal {

namespace {

// 8-bit 4:2:0 YUV is three planes: full resolution luma, then two half resolution chroma planes.
constexpr std::size_t kYuvPlaneCount = 3;

// Chroma is half the picture in each dimension.
constexpr std::size_t kChromaRatio = 2;

// One plane to read out of a frame: where its rows start, how far apart they are, and how much of
// each row is picture. Grouped because a stride says nothing without the width it belongs to.
struct PlaneCopy {
  const uint8_t* source;    ///< First visible row of the plane.
  int linesize;             ///< FFmpeg's stride between two rows, in bytes.
  std::size_t rows;         ///< Visible rows to read, at least one.
  std::size_t width_bytes;  ///< Visible bytes per row.
};

/**
 * @brief Copies one plane's visible bytes into a tightly packed destination.
 *
 * The rows are copied one at a time because a plane's stride is routinely wider than the picture:
 * FFmpeg pads every row out to an alignment so SIMD reads and edge emulation have room to overrun
 * into. Copying the plane as one block would splice one row's padding into the next row's picture,
 * shifting the tail of every row and reading past the end of the last one.
 *
 * @param copy The plane to read. Requires rows >= 1 and a non-null source.
 * @param destination First byte of the destination, which holds rows * width_bytes bytes.
 */
void copyPlane(const PlaneCopy& copy, uint8_t* destination) {
  for (std::size_t index = 0; index < copy.rows; ++index) {
    // A row is addressed through the signed stride, so a picture stored bottom-up, where the rows
    // march down in memory instead of up, still comes out in display order.
    const std::ptrdiff_t offset = static_cast<std::ptrdiff_t>(index) * copy.linesize;
    std::memcpy(destination + (index * copy.width_bytes), copy.source + offset, copy.width_bytes);
  }
}

/**
 * @brief Rejects planes that cannot be read row by row.
 * @throws std::runtime_error when a plane is absent or its stride cannot hold a visible row.
 */
void requireReadablePlanes(const std::array<PlaneCopy, kYuvPlaneCount>& planes) {
  for (const PlaneCopy& plane : planes) {
    if (plane.source == nullptr) {
      throw std::runtime_error("Decoder produced a frame with one of its YUV420P planes missing.");
    }
    const int linesize = plane.linesize < 0 ? -plane.linesize : plane.linesize;
    if (static_cast<std::size_t>(linesize) < plane.width_bytes) {
      throw std::runtime_error("Decoder produced a plane whose stride of " +
                               std::to_string(plane.linesize) + " byte(s) cannot hold a row of " +
                               std::to_string(plane.width_bytes) + " byte(s).");
    }
  }
}

/**
 * @brief Rejects a picture the render path cannot display.
 * @throws std::runtime_error when the frame is not 8-bit 4:2:0 YUV.
 */
void requireYuv420p(const AVFrame& frame) {
  const auto format = static_cast<AVPixelFormat>(frame.format);
  // Full and limited range 4:2:0 share one layout and differ only in how the samples are meant to
  // be interpreted, which no field of DecodedFrame records. Both are packed as YUV420P.
  if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P) {
    return;
  }
  const char* format_name = av_get_pix_fmt_name(format);
  throw std::runtime_error("Decoder produced pixel format '" +
                           std::string(format_name != nullptr ? format_name : "unknown") +
                           "', but only 8-bit 4:2:0 YUV can be handed to the render path.");
}

}  // namespace

DecodedFrame extractDecodedFrame(const AVFrame& frame, int64_t pts, int64_t duration) {
  requireYuv420p(frame);

  const int width = frame.width;
  const int height = frame.height;
  if (width <= 0 || height <= 0) {
    throw std::runtime_error("Decoder produced a frame with no picture area (" +
                             std::to_string(width) + "x" + std::to_string(height) + ").");
  }

  const auto picture_width = static_cast<std::size_t>(width);
  const auto picture_height = static_cast<std::size_t>(height);
  // An odd picture rounds its chroma up, because 4:2:0 shares one chroma sample per 2x2 block.
  const std::size_t chroma_width = (picture_width + 1) / kChromaRatio;
  const std::size_t chroma_height = (picture_height + 1) / kChromaRatio;

  const std::array<PlaneCopy, kYuvPlaneCount> planes{{
      {frame.data[0], frame.linesize[0], picture_height, picture_width},
      {frame.data[1], frame.linesize[1], chroma_height, chroma_width},
      {frame.data[2], frame.linesize[2], chroma_height, chroma_width},
  }};

  const std::size_t payload_size =
      picture_width * picture_height + (2 * chroma_width * chroma_height);
  requireReadablePlanes(planes);

  std::vector<uint8_t> payload(payload_size);
  std::size_t offset = 0;
  for (const PlaneCopy& plane : planes) {
    copyPlane(plane, payload.data() + offset);
    offset += plane.rows * plane.width_bytes;
  }

  const VideoFrameGeometry geometry{
      .width = width,
      .height = height,
      .pixel_format = PixelFormat::YUV420P,
      .linesize = {width, static_cast<int>(chroma_width), static_cast<int>(chroma_width), 0},
  };
  const FrameTiming timing{.pts = pts, .duration = duration};
  return DecodedFrame(timing, geometry, std::move(payload));
}

}  // namespace iptv::decoder::internal