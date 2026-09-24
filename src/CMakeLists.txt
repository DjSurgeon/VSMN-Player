# Core library definition
add_library(iptv_core STATIC
    network/http_client.cpp
    network/http_init.cpp
    network/network_component.cpp
    manifest/playlist_attribute_scanner.cpp
    manifest/playlist_m3u8_parser.cpp
    abr/abr_manager.cpp
    player/playback_orchestrator.cpp
)
target_link_libraries(iptv_core PUBLIC project_config CURL::libcurl)

# Main executable
add_executable(iptv_player main.cpp)
target_link_libraries(iptv_player PRIVATE iptv_core)
