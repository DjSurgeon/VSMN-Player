#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace iptv::demux {

struct DemuxedPacket {
  int stream_index;
  std::vector<uint8_t> data;
  int64_t pts;
};

enum class StreamType { Video, Audio, Unknown };

class TsDemuxer {
public:
  TsDemuxer() = default;

  std::vector<DemuxedPacket> demux(
      std::span<const uint8_t> ts_data,
      int* video_stream_index = nullptr,
      int* audio_stream_index = nullptr);
};

} // namespace iptv::demux