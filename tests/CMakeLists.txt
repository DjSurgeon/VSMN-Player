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
target_link_libraries(unit_tests PRIVATE GTest::gmock GTest::gtest_main iptv_core project_config httplib::httplib)

add_executable(integration_tests
    integration/test_network_subsystem.cpp
)
target_link_libraries(integration_tests PRIVATE GTest::gmock GTest::gtest_main iptv_core project_config httplib::httplib)

include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(integration_tests)

if(ENABLE_STRESS_TESTS)
    add_executable(stress_tests
        stress/network/test_network_stress.cpp
    )
    target_link_libraries(stress_tests PRIVATE GTest::gmock GTest::gtest_main iptv_core project_config httplib::httplib)
    gtest_discover_tests(stress_tests)
endif()