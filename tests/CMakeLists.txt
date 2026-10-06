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
    unit/concurrency/concurrent_queue_fixture.cpp
    unit/concurrency/test_concurrent_queue.cpp
    unit/concurrency/test_concurrent_queue_cancellation.cpp
    unit/concurrency/test_concurrent_queue_stress.cpp
    unit/decoder/mpeg4_source.cpp
    unit/decoder/test_decoder_interfaces.cpp
    unit/decoder/test_ffmpeg_audio_decoder.cpp
    unit/decoder/test_ffmpeg_decoder.cpp
    unit/decoder/test_ffmpeg_frame_extractor.cpp
    unit/decoder/test_decoder_frame.cpp
    unit/decoder/test_decoder_codec_info.cpp
    unit/abr/test_abr_manager.cpp
    unit/network/test_network_component.cpp
    unit/player/test_playback_orchestrator.cpp
)
target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
    # The frame extractor is an internal helper, so its own test reaches into the decoder module.
    ${PROJECT_SOURCE_DIR}/src/decoder
)
target_link_libraries(unit_tests PRIVATE GTest::gmock GTest::gtest_main iptv_core project_config httplib::httplib)

add_executable(integration_tests
    integration/network_subsystem_fixture.cpp
    integration/test_network_subsystem.cpp
    integration/test_network_metrics.cpp
)
target_include_directories(integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(integration_tests PRIVATE GTest::gmock GTest::gtest_main iptv_core project_config httplib::httplib)

include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(integration_tests)

if(ENABLE_STRESS_TESTS)
    add_executable(stress_tests
        stress/network/test_network_stress.cpp
        stress/player/chaos_manifest.cpp
        stress/player/chaos_planning.cpp
        stress/player/chaos_transport.cpp
        stress/player/test_orchestrator_chaos.cpp
        stress/decoder/decoder_stress_test.cpp
    )
    target_link_libraries(stress_tests PRIVATE GTest::gmock GTest::gtest_main iptv_core project_config httplib::httplib)
    target_include_directories(stress_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/stress
    )
    gtest_discover_tests(stress_tests)
endif()