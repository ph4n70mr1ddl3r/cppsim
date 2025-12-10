# Test executable
add_executable(poker_tests)

target_sources(poker_tests
  PRIVATE
    placeholder_test.cpp
)

# Link dependencies
target_link_libraries(poker_tests
  PRIVATE
    poker_common
    GTest::gtest
    GTest::gtest_main
)

# Set compile features
target_compile_features(poker_tests PRIVATE cxx_std_17)

# Enable testing - ALREADY ENABLED IN ROOT
include(GoogleTest)
gtest_discover_tests(poker_tests)
