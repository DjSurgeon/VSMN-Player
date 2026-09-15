# Testing configuration

# Tests will be added in Week 1-2
# Placeholder for GTest integration

# Example when we add GTest:
# find_package(GTest REQUIRED)
# add_executable(run_tests network/test_http_client.cpp)
# target_link_libraries(run_tests GTest::GTest GTest::Main)
# add_test(NAME AllTests COMMAND run_tests)
find_package(httplib REQUIRED)
add_executable(unit_tests
    unit/test_sanity.cpp
    unit/network/test_http_response.cpp
    unit/network/test_retry_policy.cpp
    unit/network/test_i_http_client.cpp
    unit/network/test_http_client_advanced.cpp
    unit/network/test_http_client_errors.cpp
    unit/manifest/test_playlist_parse_result.cpp
    unit/manifest/test_playlist_attribute_scanner.cpp
    unit/manifest/test_playlist_m3u8_parser.cpp
    unit/common/test_concurrent_queue.cpp
    unit/abr/test_abr_manager.cpp
    unit/network/test_network_component.cpp
    unit/player/test_playback_orchestrator.cpp
)
target_link_libraries(unit_tests PRIVATE GTest::gmock GTest::gtest_main iptv_core httplib::httplib)
target_compile_options(unit_tests PRIVATE ${PROJECT_WARNINGS})

include(GoogleTest)
gtest_discover_tests(unit_tests)