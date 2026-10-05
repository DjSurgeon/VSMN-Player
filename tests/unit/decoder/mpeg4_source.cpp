#include "mpeg4_source.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

// The pictures are encoded at 25 frames per second, a rate every FFmpeg build supports and one
// that leaves the frame duration at a round 40 milliseconds.
namespace {

constexpr AVRational kPictureTimeBase{1, 25};
constexpr AVRational kFrameRate{25, 1};

// Every picture is intra-coded, so a packet decodes on its own without a preceding key frame.
constexpr int kGroupOfPictures = 1;

// Structure owning an FFmpeg encoder context so a failed open cannot leak it.
struct EncoderContextDeleter {
  void operator()(AVCodecContext* context) const noexcept {
    if (context != nullptr) {
      avcodec_free_context(&context);
    }
  }
};

using EncoderContextPtr = std::unique_ptr<AVCodecContext, EncoderContextDeleter>;

struct FrameDeleter {
  void operator()(AVFrame* frame) const noexcept {
    if (frame != nullptr) {
      av_frame_free(&frame);
    }
  }
};

using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

struct PacketDeleter {
  void operator()(AVPacket* packet) const noexcept {
    if (packet != nullptr) {
      av_packet_free(&packet);
    }
  }
};

using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

/**
 * @brief Opens an mpeg4 encoder sized for the pictures it will be fed.
 * @throws std::runtime_error when the encoder cannot be opened.
 */
EncoderContextPtr openEncoder(int width, int height, int max_b_frames) {
  const AVCodec* encoder = avcodec_find_encoder_by_name("mpeg4");
  if (encoder == nullptr) {
    throw std::runtime_error(
        "This FFmpeg build has no mpeg4 encoder, so the decoder tests have no "
        "compressed input to decode.");
  }

  EncoderContextPtr context(avcodec_alloc_context3(encoder));
  if (!context) {
    throw std::runtime_error("Failed to allocate an mpeg4 encoder context.");
  }
  context->width = width;
  context->height = height;
  context->pix_fmt = AV_PIX_FMT_YUV420P;
  context->time_base = kPictureTimeBase;
  context->framerate = kFrameRate;
  context->gop_size = kGroupOfPictures;
  context->max_b_frames = max_b_frames;

  if (avcodec_open2(context.get(), encoder, nullptr) < 0) {
    throw std::runtime_error("Failed to open the mpeg4 encoder.");
  }
  return context;
}

/**
 * @brief Allocates the picture handed to the encoder.
 * @throws std::runtime_error when the frame buffer cannot be allocated.
 */
FramePtr allocatePicture(int width, int height) {
  FramePtr picture(av_frame_alloc());
  if (!picture) {
    throw std::runtime_error("Failed to allocate an mpeg4 picture.");
  }
  picture->format = AV_PIX_FMT_YUV420P;
  picture->width = width;
  picture->height = height;
  if (av_frame_get_buffer(picture.get(), 0) < 0) {
    throw std::runtime_error("Failed to allocate the mpeg4 picture buffer.");
  }
  return picture;
}

/**
 * @brief Paints picture @p index with a luma ramp and chroma levels no other picture uses.
 *
 * The ramp is laid down per visible row and never per stride byte, so the encoder sees the same
 * picture regardless of the padding its buffer happens to carry.
 */
void paintPicture(AVFrame* picture, int index) {
  const int width = picture->width;
  const int height = picture->height;

  for (int row = 0; row < height; ++row) {
    uint8_t* luma = picture->data[0] +
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(picture->linesize[0]);
    for (int column = 0; column < width; ++column) {
      luma[column] = static_cast<uint8_t>((column + (index * 8)) & 0xFF);
    }
  }

  const int chroma_height = height / 2;
  const int chroma_width = width / 2;
  for (int row = 0; row < chroma_height; ++row) {
    for (int column = 0; column < chroma_width; ++column) {
      const std::size_t chroma_u =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(picture->linesize[1]) +
          static_cast<std::size_t>(column);
      const std::size_t chroma_v =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(picture->linesize[2]) +
          static_cast<std::size_t>(column);
      picture->data[1][chroma_u] = static_cast<uint8_t>((index * 16) & 0xFF);
      picture->data[2][chroma_v] = static_cast<uint8_t>(((index * 16) + 8) & 0xFF);
    }
  }
}

/**
 * @brief Moves every packet the encoder holds into @p packets and unrefs it.
 */
void drainPackets(AVCodecContext* context, AVPacket* packet,
                  std::vector<std::vector<uint8_t>>& packets) {
  while (avcodec_receive_packet(context, packet) == 0) {
    packets.emplace_back(packet->data, packet->data + packet->size);
    av_packet_unref(packet);
  }
}

/**
 * @brief Rejects a status the encoder should never return for a synthetic picture.
 * @throws std::runtime_error when the status is negative.
 */
void requireEncoderAccepted(int status, const char* stage) {
  if (status < 0) {
    throw std::runtime_error(std::string("mpeg4 encoder ") + stage + " failed with status " +
                             std::to_string(status) + ".");
  }
}

}  // namespace

namespace iptv::decoder::testing {

std::vector<std::vector<uint8_t>> encodeMpeg4Pictures(int width, int height, int picture_count,
                                                      int max_b_frames) {
  const EncoderContextPtr encoder = openEncoder(width, height, max_b_frames);
  const FramePtr picture = allocatePicture(width, height);
  const PacketPtr packet(av_packet_alloc());
  if (!packet) {
    throw std::runtime_error("Failed to allocate an mpeg4 packet.");
  }

  std::vector<std::vector<uint8_t>> packets;
  for (int index = 0; index < picture_count; ++index) {
    paintPicture(picture.get(), index);
    picture->pts = index;

    requireEncoderAccepted(avcodec_send_frame(encoder.get(), picture.get()), "frame submission");
    drainPackets(encoder.get(), packet.get(), packets);
  }

  requireEncoderAccepted(avcodec_send_frame(encoder.get(), nullptr), "flush");
  drainPackets(encoder.get(), packet.get(), packets);
  return packets;
}

}  // namespace iptv::decoder::testing