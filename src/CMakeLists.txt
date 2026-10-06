# Media decoding dependency managed by Conan 2
find_package(ffmpeg REQUIRED)

# Core library definition
add_library(iptv_core STATIC
    decoder/ffmpeg_audio_decoder.cpp
    decoder/ffmpeg_decoder.cpp
    decoder/internal/ffmpeg_buffers.cpp
    decoder/internal/ffmpeg_frame_extractor.cpp
    network/http_client.cpp
    network/http_init.cpp
    network/internal/http_client_impl.cpp
    network/internal/transfer_callbacks.cpp
    network/network_component.cpp
    manifest/playlist_attribute_scanner.cpp
    manifest/playlist_m3u8_parser.cpp
    manifest/internal/line_reader.cpp
    manifest/internal/playlist_line_parser.cpp
    manifest/internal/playlist_state.cpp
    manifest/internal/playlist_tag_handlers.cpp
    abr/abr_manager.cpp
    player/internal/url_resolver.cpp
    player/playback_orchestrator.cpp
)
target_include_directories(iptv_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder
    ${CMAKE_CURRENT_SOURCE_DIR}/manifest
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/player
)
target_link_libraries(iptv_core PUBLIC
    CURL::libcurl
    ffmpeg::avcodec
    ffmpeg::avformat
    ffmpeg::avutil
    ffmpeg::swresample
    project_config
)

# Main executable
add_executable(iptv_player main.cpp)
target_link_libraries(iptv_player PRIVATE iptv_core)
