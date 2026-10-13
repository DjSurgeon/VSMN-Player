#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace iptv::demux {

/**
 * @brief A demuxed elementary stream packet with timing information.
 *
 * Holds the stream index, raw data bytes, and presentation/display timestamps
 * for a single TS packet group decoded from a transport stream section.
 */
struct DemuxedPacket {
  /**
   * @brief Index of the elementary stream this packet belongs to.
   *        Useful for routing to the correct decoder (video vs audio).
   */
  int stream_index;
  /**
   * @brief Raw compressed bytes of the TS packet group.
   *        Caller retains ownership; the data must outlive any usage.
   */
  std::vector<uint8_t> data;
  /**
   * @brief Presentation Timestamp in microseconds, relative to the stream start.
   *        Used for PTS ordering in the frame buffer.
   */
  int64_t pts;
  /**
   * @brief Decoding Timestamp in microseconds, indicating when the sample was
   *        decoded. Essential for B-frame reordering correctness.
   */
  int64_t dts;
};

/**
 * @brief Identifies the type of elementary stream carried in a TS packet.
 *
 * Used by `TsDemuxer::demux()` to signal which decoder should process
 * the returned packets.
 */
enum class StreamType { Video, Audio, Unknown };

/**
 * @brief Demuxes raw MPEG-TS payload into elementary stream packets.
 *
 * Parses the transport stream data and separates out video, audio, and
 * unknown elementary streams based on PID filtering and PAT/PMT analysis.
 *
 * @param ts_data Raw MPEG-TS bytes to demux. Expected to be a valid
 *                transport stream section.
 * @param[out] video_stream_index Set to the index of the video stream
 *                                within the TS, if one is found. May be
 *                                left unchanged if no video stream is present.
 * @param[out] audio_stream_index Set to the index of the audio stream
 *                                within the TS, if one is found. May be
 *                                left unchanged if no audio stream is present.
 * @return A vector of `DemuxedPacket`, one per elementary stream found.
 *         Empty vector if no packets were recognized in the input data.
 * @throws std::invalid_argument if `ts_data` is empty or contains no
 *         recognizable PSI/SI tables.
 */
class TsDemuxer {
public:
  TsDemuxer() = default;

  /**
   * @brief Demuxes a single TS data block into elementary stream packets.
   * @param ts_data Raw MPEG-TS bytes to demux.
   * @param[out] video_stream_index Set to the index of the video stream
   *                                within the TS, if one is found.
   * @param[out] audio_stream_index Set to the index of the audio stream
   *                                within the TS, if one is found.
   * @return Vector of demuxed packets classified by stream type.
   */
  std::vector<DemuxedPacket> demux(
      std::span<const uint8_t> ts_data,
      int* video_stream_index = nullptr,
      int* audio_stream_index = nullptr);
};

} // namespace iptv::demux