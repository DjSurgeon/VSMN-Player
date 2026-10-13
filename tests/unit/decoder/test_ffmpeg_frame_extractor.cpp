#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "internal/ffmpeg_frame_extractor.hpp"

namespace {

using iptv::decoder::DecodedFrame;
using iptv::decoder::MediaType;
using iptv::decoder::PixelFormat;
using iptv::decoder::internal::extractDecodedFrame;

// FFmpeg pads the rows of a plane out to an alignment so SIMD reads and edge emulation can overrun
// into. Asking for a wide alignment guarantees the planes under test are padded, which is the case
// a flat copy of a plane gets wrong.
constexpr int kPlaneAlignment = 64;

// A value no painted picture byte can hold, so padding that leaked into the payload is visible.
constexpr uint8_t kPaddingMarker = 0xEE;

/**
 * @brief Value painted at (row, column) of a picture.
 *
 * Distinct per row, so a copy that shifted rows shows up as a mismatch, and distinct per column,
 * so a copy that transposed anything shows up as one too.
 */
[[nodiscard]] uint8_t paintedValue(int row, int column) {
  return static_cast<uint8_t>(((row + 1) * 16) + (column % 16));
}

struct FrameDeleter {
  void operator()(AVFrame* frame) const noexcept {
    if (frame != nullptr) {
      av_frame_free(&frame);
    }
  }
};

using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

/**
 * @brief Allocates a padded YUV420P frame and paints every byte of it, padding included.
 */
FramePtr makePaddedFrame(int width, int height) {
  FramePtr frame(av_frame_alloc());
  if (!frame) {
    throw std::runtime_error("Failed to allocate the AVFrame under test.");
  }
  frame->format = AV_PIX_FMT_YUV420P;
  frame->width = width;
  frame->height = height;
  if (av_frame_get_buffer(frame.get(), kPlaneAlignment) < 0) {
    throw std::runtime_error("Failed to allocate the AVFrame buffer under test.");
  }

  const std::array<std::size_t, 3> row_bytes{static_cast<std::size_t>(width),
                                             static_cast<std::size_t>((width + 1) / 2),
                                             static_cast<std::size_t>((width + 1) / 2)};
  const std::array<std::size_t, 3> rows{static_cast<std::size_t>(height),
                                        static_cast<std::size_t>((height + 1) / 2),
                                        static_cast<std::size_t>((height + 1) / 2)};

  for (std::size_t plane = 0; plane < row_bytes.size(); ++plane) {
    const std::size_t stride = static_cast<std::size_t>(frame->linesize[plane]);
    for (std::size_t row = 0; row < rows[plane]; ++row) {
      uint8_t* line = frame->data[plane] + (row * stride);
      std::memset(line, kPaddingMarker, stride);
      for (std::size_t column = 0; column < row_bytes[plane]; ++column) {
        line[column] = paintedValue(static_cast<int>(row), static_cast<int>(column));
      }
    }
  }
  return frame;
}

/**
 * @brief Builds the payload a correct copy must produce: plane after plane, row by row.
 *
 * Written from the picture itself rather than from the frame, so it describes what the payload has
 * to be and not how the code under test gets there.
 */
std::vector<uint8_t> expectedPayload(int width, int height) {
  // Luma, then two quarter sized chroma planes: the payload order the render path expects.
  const std::size_t chroma_width = static_cast<std::size_t>((width + 1) / 2);
  const std::size_t chroma_height = static_cast<std::size_t>((height + 1) / 2);
  const std::array<std::pair<std::size_t, std::size_t>, 3> planes{{
      {static_cast<std::size_t>(height), static_cast<std::size_t>(width)},
      {chroma_height, chroma_width},
      {chroma_height, chroma_width},
  }};

  std::vector<uint8_t> expected;
  expected.reserve((static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) +
                   (2 * chroma_width * chroma_height));
  for (const auto& [rows, row_bytes] : planes) {
    for (std::size_t row = 0; row < rows; ++row) {
      for (std::size_t column = 0; column < row_bytes; ++column) {
        expected.push_back(paintedValue(static_cast<int>(row), static_cast<int>(column)));
      }
    }
  }
  return expected;
}

TEST(FFmpegFrameExtractorTest, CopiesVisibleRowsOnlyAndLeavesThePaddingBehind) {
  constexpr int kWidth = 24;
  constexpr int kHeight = 16;
  const FramePtr frame = makePaddedFrame(kWidth, kHeight);

  // The premise: every plane is padded wider than the picture, which is what makes the copy
  // row by row rather than a single block.
  for (int plane = 0; plane < 3; ++plane) {
    ASSERT_GT(frame->linesize[plane], (plane == 0) ? kWidth : (kWidth / 2));
  }

  DecodedFrame decoded = extractDecodedFrame(*frame, 0, 40);

  // Row for row: the padding between rows is never spliced into the next row's picture.
  EXPECT_EQ(decoded.data, expectedPayload(kWidth, kHeight));
  // Spelled out at the seam: byte kWidth is the first visible byte of row 1, not row 0's padding.
  ASSERT_GT(decoded.data.size(), static_cast<std::size_t>(kWidth));
  EXPECT_EQ(decoded.data[kWidth], paintedValue(1, 0));
  EXPECT_NE(decoded.data[kWidth], kPaddingMarker);
}

TEST(FFmpegFrameExtractorTest, PacksPlanesBackToBackAndDeclaresTheirPackedStrides) {
  constexpr int kWidth = 24;
  constexpr int kHeight = 16;
  const FramePtr frame = makePaddedFrame(kWidth, kHeight);

  const DecodedFrame decoded = extractDecodedFrame(*frame, 0, 40);

  // Luma, then chroma, with no gap and no slack between them.
  const std::size_t luma = static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight);
  const std::size_t chroma = luma / 4;
  EXPECT_EQ(decoded.data.size(), luma + (2 * chroma));
  EXPECT_EQ(decoded.media_type, MediaType::Video);
  EXPECT_EQ(decoded.pixel_format, PixelFormat::YUV420P);
  EXPECT_EQ(decoded.width, kWidth);
  EXPECT_EQ(decoded.height, kHeight);
  EXPECT_EQ(decoded.linesize,
            (std::array<int, DecodedFrame::kLinesizeSlots>{kWidth, kWidth / 2, kWidth / 2, 0}));
  // Declared strides must address the packed payload: the last row must not read past its end.
  EXPECT_LE(static_cast<std::size_t>(decoded.linesize[0]) * static_cast<std::size_t>(kHeight),
            decoded.data.size());
}

TEST(FFmpegFrameExtractorTest, ReadsRowsInDisplayOrderWhenTheStrideIsNegative) {
  constexpr int kWidth = 8;
  constexpr int kHeight = 4;
  // FFmpeg reports a negative stride for pictures stored bottom-up: display row 0 sits at the far
  // end of the buffer and row r sits stride bytes before row r - 1. The payload must still come out
  // top row first.
  const FramePtr bottom_up(av_frame_alloc());
  ASSERT_TRUE(bottom_up != nullptr);
  bottom_up->format = AV_PIX_FMT_YUV420P;
  bottom_up->width = kWidth;
  bottom_up->height = kHeight;
  ASSERT_EQ(av_frame_get_buffer(bottom_up.get(), kPlaneAlignment), 0);

  const std::array<std::size_t, 3> row_bytes{static_cast<std::size_t>(kWidth),
                                             static_cast<std::size_t>(kWidth / 2),
                                             static_cast<std::size_t>(kWidth / 2)};
  const std::array<std::size_t, 3> rows{static_cast<std::size_t>(kHeight),
                                        static_cast<std::size_t>(kHeight / 2),
                                        static_cast<std::size_t>(kHeight / 2)};
  for (std::size_t plane = 0; plane < row_bytes.size(); ++plane) {
    const std::size_t stride = static_cast<std::size_t>(bottom_up->linesize[plane]);
    uint8_t* const buffer_start = bottom_up->data[plane];
    // A negative stride puts display row 0 at the far end of the buffer, and row r sits stride
    // bytes before row r - 1.
    bottom_up->linesize[plane] = -bottom_up->linesize[plane];
    bottom_up->data[plane] += (rows[plane] - 1) * stride;
    for (std::size_t row = 0; row < rows[plane]; ++row) {
      uint8_t* line = buffer_start + ((rows[plane] - 1 - row) * stride);
      std::memset(line, kPaddingMarker, stride);
      for (std::size_t column = 0; column < row_bytes[plane]; ++column) {
        line[column] = paintedValue(static_cast<int>(row), static_cast<int>(column));
      }
    }
  }

  EXPECT_EQ(extractDecodedFrame(*bottom_up, 0, 40).data, expectedPayload(kWidth, kHeight));
}

TEST(FFmpegFrameExtractorTest, AcceptsFullRangeYuv420pAsYuv420p) {
  constexpr int kWidth = 16;
  constexpr int kHeight = 16;
  const FramePtr frame = makePaddedFrame(kWidth, kHeight);
  frame->format = AV_PIX_FMT_YUVJ420P;

  // Full and limited range share one layout; no DecodedFrame field records which one this is.
  const DecodedFrame decoded = extractDecodedFrame(*frame, 0, 40);

  EXPECT_EQ(decoded.pixel_format, PixelFormat::YUV420P);
  EXPECT_EQ(decoded.data, expectedPayload(kWidth, kHeight));
}

TEST(FFmpegFrameExtractorTest, StampsTheTimingItIsGiven) {
  const FramePtr frame = makePaddedFrame(16, 16);

  const DecodedFrame decoded = extractDecodedFrame(*frame, 1234, 40);

  EXPECT_EQ(decoded.pts, 1234);
  EXPECT_EQ(decoded.duration, 40);
}

TEST(FFmpegFrameExtractorTest, RefusesAPixelFormatTheRenderPathCannotShow) {
  const FramePtr frame = makePaddedFrame(16, 16);
  frame->format = AV_PIX_FMT_YUV444P;

  // Reshaping the picture would need a converter this build does not carry, so it is reported
  // rather than silently mislabelled.
  EXPECT_THROW({ (void)extractDecodedFrame(*frame, 0, 40); }, std::runtime_error);
}

TEST(FFmpegFrameExtractorTest, RefusesAFrameWithNoPictureArea) {
  const FramePtr frame = makePaddedFrame(16, 16);
  frame->width = 0;

  EXPECT_THROW({ (void)extractDecodedFrame(*frame, 0, 40); }, std::runtime_error);
}

TEST(FFmpegFrameExtractorTest, RefusesAFrameWhosePlaneIsAbsent) {
  const FramePtr frame = makePaddedFrame(16, 16);
  frame->data[2] = nullptr;

  EXPECT_THROW({ (void)extractDecodedFrame(*frame, 0, 40); }, std::runtime_error);
}

TEST(FFmpegFrameExtractorTest, RefusesAStrideThatCannotHoldOneRow) {
  const FramePtr frame = makePaddedFrame(16, 16);
  frame->linesize[0] = 4;

  // Copying 16 bytes per row from a 4 byte stride would walk off the end of the plane.
  EXPECT_THROW({ (void)extractDecodedFrame(*frame, 0, 40); }, std::runtime_error);
}

}  // namespace