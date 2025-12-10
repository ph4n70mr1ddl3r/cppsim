# Story 1.1: CMake Project Structure & Dependencies

**Status:** ready-for-dev  
**Epic:** Epic 1 - Project Foundation & Build Infrastructure  
**Story ID:** 1.1  
**Estimated Effort:** Medium (4-6 hours)

---

## User Story

As a **developer**,  
I want **a working CMake build system with all dependencies managed via FetchContent**,  
So that **I can build the project on any platform without manual dependency installation**.

---

## Acceptance Criteria

**Given** a fresh clone of the repository  
**When** I run `cmake -B build -S .`  
**Then** CMake configures successfully and fetches Boost, nlohmann/json, and Google Test  
**And** all dependencies are pinned to specific versions  
**And** the build system creates separate targets for server, client, common library, and tests

---

## Technical Requirements

### Primary Technologies
- **Build System:** CMake 3.15+ (minimum version as specified in architecture)
- **C++ Standard:** C++17 (as per architecture requirements)
- **Dependencies:**
  - Boost.Beast (WebSocket implementation)
  - Boost.Asio (async I/O event loop)
  - nlohmann/json (JSON serialization/deserialization)
  - Google Test (testing framework)

### Dependency Management Strategy
- Use CMake **FetchContent** for all dependencies (no manual installation required)
- Pin all dependencies to **specific versions** for reproducibility
- Ensure cross-platform compatibility (Windows, Linux, macOS)

---

## Architecture Compliance

### Monorepo Structure
Per the architecture document, this project must adhere to a **monorepo** structure containing:
- **Server** (poker server executable)
- **Client** (bot client executable)
- **Common** (shared library for protocol definitions, poker rules, hand evaluation logic)
- **Tests** (unit tests, integration tests, stress tests)

### CMake Target Requirements
The build system must create **separate targets**:
1. `poker_server` - Server executable
2. `poker_client` - Client executable
3. `poker_common` - Shared library (static or shared)
4. `poker_tests` - Test executable

### Compiler Requirements
- **Compiler Flags:** Enable pedantic warnings (`-Wall -Wextra -Wpedantic` for GCC/Clang, `/W4` for MSVC)
- **Warnings as Errors:** Treat warnings as errors to enforce code quality
- **C++17 Standard:** Explicitly set `CMAKE_CXX_STANDARD 17`

---

## Library & Framework Requirements

### Boost Libraries
**Version:** Boost 1.82.0 or later (recommended latest stable)

**Components Needed:**
- `Boost.Beast` - For WebSocket server/client implementation
- `Boost.Asio` - For async I/O and event loop
- `Boost.System` - Required dependency for Asio

**Important:** Boost can be fetched via CMake FetchContent or system-installed. For maximum portability, **use FetchContent** to ensure consistent versions across all platforms.

**CMake FetchContent Example:**
```cmake
include(FetchContent)

FetchContent_Declare(
  Boost
  URL https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz
  URL_HASH SHA256=<hash>
)
set(BOOST_INCLUDE_LIBRARIES beast asio system)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_MakeAvailable(Boost)
```

### nlohmann/json
**Version:** 3.11.2 or later

**Purpose:** JSON serialization/deserialization for protocol messages

**CMake FetchContent Example:**
```cmake
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)
```

### Google Test
**Version:** 1.14.0 or later

**Purpose:** Unit testing framework

**CMake FetchContent Example:**
```cmake
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
```

---

## File Structure Requirements

### Expected Directory Layout (Per Architecture)
```
cppsim/
├── CMakeLists.txt (root)
├── src/
│   ├── server/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp (placeholder)
│   ├── client/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp (placeholder)
│   └── common/
│       ├── CMakeLists.txt
│       └── (protocol, poker_rules, hand_evaluator headers/sources will come later)
├── tests/
│   ├── CMakeLists.txt
│   ├── unit/
│   ├── integration/
│   └── stress/
├── cmake/
│   └── (optional helper modules)
└── docs/
    └── (documentation)
```

### This Story's Scope
For Story 1.1, create:
- Root `CMakeLists.txt` with FetchContent dependency management
- `src/server/CMakeLists.txt` with placeholder `main.cpp`
- `src/client/CMakeLists.txt` with placeholder `main.cpp`
- `src/common/CMakeLists.txt` (empty for now, will be populated in later stories)
- `tests/CMakeLists.txt` (basic Google Test setup)

**Do not create:**
- Directory structure (Story 1.2 handles this)
- Coding standards enforcement (Story 1.3 handles this)

---

## Testing Requirements

### Build Verification
After implementation, the following must succeed:
1. **Configuration:** `cmake -B build -S .` should configure without errors
2. **Build:** `cmake --build build` should build all targets successfully
3. **Dependency Fetch:** All dependencies should download and integrate automatically
4. **Cross-Platform:** Test on at least one platform (Linux/Windows/macOS)

### Expected Outputs
- `build/src/server/poker_server` (or `.exe` on Windows)
- `build/src/client/poker_client`
-  build/tests/poker_tests`

### Placeholder Functionality
- Server and client executables should print "Hello from poker server!" and "Hello from poker client!" respectively, then exit successfully
- Tests should have at least one trivial passing test (e.g., `TEST(Placeholder, Passes) { EXPECT_TRUE(true); }`)

---

## Implementation Notes

### CMake Best Practices
1. **Use Modern CMake** (targets and properties, not global variables)
2. **Avoid `include_directories()`** - use `target_include_directories()` instead
3. **Link libraries with visibility** - `PUBLIC`, `PRIVATE`, `INTERFACE` keywords
4. **Set compile features, not flags directly** - e.g., `target_compile_features(target PUBLIC cxx_std_17)`

### Common Pitfalls to Avoid
- **Don't hardcode paths** - use CMake variables like `${CMAKE_SOURCE_DIR}`
- **Don't use outdated Boost download methods** - FetchContent is the modern approach
- **Avoid mixing FetchContent with find_package** for the same library - choose one strategy

### Performance Optimization
- **Enable parallel builds** - Ensure CMake supports `cmake --build build --parallel`
- **Use Ninja generator** if available - `cmake -G Ninja -B build -S .` for faster builds

---

## Definition of Done

- [ ] Root `CMakeLists.txt` created with FetchContent for all dependencies
- [ ] Separate CMakeLists.txt for server, client, common, and tests
- [ ] Placeholder `main.cpp` files created for server and client
- [ ] `cmake -B build -S .` configures successfully
- [ ] `cmake --build build` builds all targets without errors or warnings
- [ ] Server and client executables run and print placeholder messages
- [ ] Tests executable runs and passes placeholder test
- [ ] Dependencies are pinned to specific versions
- [ ] Cross-platform build verified on at least one platform

---

## Context & Dependencies

### Depends On
- **None** (This is the first story in the project)

### Blocks
- **Story 1.2:** Project Directory Structure (needs CMake setup first)
- **All Epic 2+ stories** require this foundation

### Related Stories
- Story 1.3 will add clang-format and stricter compiler warnings
- Epic 2 stories will add actual protocol and network code to the common library

---

## Developer Notes

### Why FetchContent?
FetchContent is the modern CMake approach (since CMake 3.11) that:
- Eliminates manual dependency installation
- Ensures version consistency across all developers and CI systems
- Works seamlessly with cross-platform builds
- Integrates dependencies as CMake targets

### Alternative: System-Installed Dependencies
If team preference is to use system-installed Boost (e.g., `apt install libboost-all-dev` on Ubuntu), you may use `find_package(Boost REQUIRED COMPONENTS beast system)` instead of FetchContent. However, this sacrifices reproducibility and cross-platform portability.

### Estimated Complexity
**Low-Medium** - Standard CMake setup, well-documented dependencies, but requires attention to:
- Boost FetchContent can be slow (large download) - consider caching mechanisms
- Ensuring cross-platform compatibility
- Correct dependency ordering

---

## Resources

### Official Documentation
- CMake FetchContent: https://cmake.org/cmake/help/latest/module/FetchContent.html
- Boost.Beast Documentation: https://www.boost.org/doc/libs/1_82_0/libs/beast/doc/html/index.html
- nlohmann/json GitHub: https://github.com/nlohmann/json
- Google Test Primer: https://google.github.io/googletest/primer.html

### Example CMake Projects
- Boost.Beast Examples: https://github.com/boostorg/beast/tree/develop/example
- Modern CMake Examples: https://github.com/pr0g/cmake-examples

---

**This story is ready for development. Once complete, proceed to Story 1.2: Project Directory Structure.**
