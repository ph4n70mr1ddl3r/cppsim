cmake_minimum_required(VERSION 3.15)
project(cppsim VERSION 0.1.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include compiler warnings module
include(cmake/CompilerWarnings.cmake)

# Create interface library for project-wide settings
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)

# Create interface library for compiler warnings
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

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
  URL https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_UNDERSCORE}.tar.gz
  URL_HASH SHA256=c0685b68dd44cc46574cce86c4e17c0f611b15e195be9848dfd0769a0a207628
)
FetchContent_MakeAvailable(Boost)

if(NOT TARGET Boost::headers)
  add_library(boost_headers_interface INTERFACE)
  target_include_directories(boost_headers_interface INTERFACE ${boost_SOURCE_DIR})
  add_library(Boost::headers ALIAS boost_headers_interface)
endif()

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

# Apply project-wide options and warnings to all targets
# Using PRIVATE linkage because these are executables (server, client, tests) that don't
# export their interface. If creating new libraries, consider PUBLIC linkage to propagate
# warnings to consumers.
target_link_libraries(poker_server PRIVATE project_options project_warnings)
target_link_libraries(poker_client PRIVATE project_options project_warnings)
# poker_common is a STATIC library
# Consumers (server/client) already link project_warnings above, which is sufficient
# poker_common links project_warnings internally
target_link_libraries(poker_tests PRIVATE project_options project_warnings)

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
message(STATUS "  - poker_server_lib (server static library)")
message(STATUS "  - poker_client (client executable)")
message(STATUS "  - poker_common (static library)")
message(STATUS "  - poker_tests (test executable)")
message(STATUS "===================================")
message(STATUS "")
