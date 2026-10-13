Write a complete integration test for the decoding pipeline: `tests/integration/test_pipeline_integration.cpp`.

Objective:
Test the full pipeline: MPEG-TS segment -> extracted packets -> decoded frames (video + audio).

Scope:
- Do NOT hallucinate `MpegTsDemuxer` or `Packet` classes; they don't exist in our project.
- Use FFmpeg's `libavformat` (`avformat_open_input`, `av_read_frame`, etc.) directly in the test to demux a synthetic MPEG-TS stream in memory.
- Feed the extracted AVPacket data (`packet->data`, `packet->size`) as `std::span<const uint8_t>` to our `FFmpegDecoder` (for video) and `FFmpegAudioDecoder` (for audio).
- Assert that we get valid `DecodedFrame`s out (correct video dimensions, audio sample rate 48kHz, stereo, PCM, matching PTS).
- Generate a synthetic MPEG-TS segment in memory containing interleaved H.264 video and AAC audio (use `libavformat` to mux synthetic frames into TS).
- Create edge cases: dropped packets, malformed PES, extreme AV sync drift.
- Use GoogleTest (`#include <gtest/gtest.h>`).

Run this without building (`--auto --no-build`). Read `include/iptv/decoder/*.hpp` and `tests/unit/decoder/aac_source.cpp` to understand our APIs before writing the test.
