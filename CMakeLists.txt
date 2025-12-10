cmake_minimum_required(VERSION 3.15)
project(cppsim VERSION 0.1.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable basic compiler warnings (Story 1.3 will add more)
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include FetchContent module  
include(FetchContent)

# =============================================================================
# Dependency Management
# =============================================================================

# Boost - Use FetchContent for cross-platform builds (per AC requirement)
set(BOOST_VERSION "1.83.0")
set(BOOST_VERSION_UNDERSCORE "1_83_0")
message(STATUS "Fetching Boost ${BOOST_VERSION}...")
FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}.tar.gz
  URL_HASH SHA256=0c6049764e80aa32754acd7d4f179fd5551d8172a83b71532ae093e7384e98da
)
FetchContent_MakeAvailable(Boost)

# nlohmann/json
message(STATUS "Fetching nlohmann/json...")
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Google Test
message(STATUS "Fetching Google Test...")
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# =============================================================================
# Subdirectories
# =============================================================================

add_subdirectory(src/common)
add_subdirectory(src/server)
add_subdirectory(src/client)

# Enable testing at root (must be before adding tests subdirectory)
enable_testing()
add_subdirectory(tests)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "===================================")
message(STATUS "cppsim - C++ Poker Server Simulation")
message(STATUS "===================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Boost Version: ${Boost_VERSION}")
message(STATUS "Targets:")
message(STATUS "  - poker_server (server executable)")
message(STATUS "  - poker_client (client executable)")
message(STATUS "  - poker_common (shared library)")
message(STATUS "  - poker_tests (test executable)")
message(STATUS "===================================")
message(STATUS "")
