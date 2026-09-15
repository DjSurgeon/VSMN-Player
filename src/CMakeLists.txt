# Core library definition
add_library(iptv_core STATIC
    network/http_curl_callbacks.cpp
    network/http_client.cpp
    network/http_init.cpp
    network/network_component.cpp
    manifest/playlist_attribute_scanner.cpp
    manifest/playlist_m3u8_parser.cpp
)
target_include_directories(iptv_core PUBLIC ../include)
target_compile_options(iptv_core PRIVATE ${PROJECT_WARNINGS})
target_link_libraries(iptv_core PUBLIC CURL::libcurl)

# Main executable
add_executable(iptv_player main.cpp)
target_compile_options(iptv_player PRIVATE ${PROJECT_WARNINGS})
target_link_libraries(iptv_player PRIVATE iptv_core)
