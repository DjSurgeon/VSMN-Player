# Development Log

## Week 2 Completion
- **Decoder Hardening**: Achieved >85% code coverage across `ffmpeg_decoder.cpp`, `ffmpeg_audio_decoder.cpp`, and `ffmpeg_buffers.cpp`. Unreachable FFmpeg OOM allocations were successfully isolated and ignored to focus testing on business logic.
- **Sanitizers**: Fully integrated AddressSanitizer (ASan) and ThreadSanitizer (TSan) into the CI pipeline. Clean pass achieved (httplib threads suppressed appropriately).
- **FFmpeg Integration**: Completed automatic codec detection from bitstream (`av_probe_input_format`). Resampling logic integrated and tested rigorously using white-box and black-box strategies.
- **Stability**: Extreme inputs and corrupted frames are handled cleanly via typed `DecoderException` without leaking AVFrames or packets.

