# Test executable
add_executable(poker_tests)

target_sources(poker_tests
  PRIVATE
    placeholder_test.cpp
    unit/protocol_test.cpp
    integration/websocket_server_test.cpp
    integration/handshake_test.cpp
)

# Link dependencies
target_link_libraries(poker_tests
  PRIVATE
    poker_common
    poker_server_lib
    GTest::gtest
    GTest::gtest_main
    Boost::headers
)

# Set compile features
target_compile_features(poker_tests PRIVATE cxx_std_17)

target_include_directories(poker_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Enable testing - ALREADY ENABLED IN ROOT
include(GoogleTest)
gtest_discover_tests(poker_tests)
