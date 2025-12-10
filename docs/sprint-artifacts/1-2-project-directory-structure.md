# Story 1.2: Project Directory Structure

**Status:** ready-for-dev  
**Epic:** Epic 1 - Project Foundation & Build Infrastructure  
**Story ID:** 1.2  
**Estimated Effort:** Small (2-3 hours)

---

## User Story

As a **developer**,  
I want **the complete directory structure following the architecture specification**,  
So that **I have organized locations for all code components**.

---

## Acceptance Criteria

**Given** the repository root  
**When** I inspect the directory structure  
**Then** I see `src/server/`, `src/client/`, `src/common/`, `tests/unit/`, `tests/integration/`, `tests/stress/`, `docs/`, and `cmake/` directories  
**And** each directory has appropriate CMakeLists.txt files  
**And** placeholder main.cpp files exist for server and client

---

## Technical Requirements

### Complete Directory Tree

Per the architecture specification, create the following directory structure:

```
cppsim/
├── CMakeLists.txt                 # Root CMake (from Story 1.1)
├── README.md                      # Project overview
├── .gitignore                     # Git ignore patterns
├── src/
│   ├── server/
│   │   ├── CMakeLists.txt         # Server build config (from Story 1.1)
│   │   ├── main.cpp               # Server entry point (from Story 1.1)
│   │   ├── connection_manager.hpp # (Future stories)
│   │   ├── connection_manager.cpp
│   │   └── (other server components...)
│   ├── client/
│   │   ├── CMakeLists.txt         # Client build config (from Story 1.1)
│   │   ├── main.cpp               # Client entry point (from Story 1.1)
│   │   ├── strategy_engine.hpp    # (Future stories)
│   │   ├── strategy_engine.cpp
│   │   └── (other client components...)
│   └── common/
│       ├── CMakeLists.txt         # Common library build (from Story 1.1)
│       ├── protocol/
│       │   ├── protocol.hpp       # (Epic 2)
│       │   └── protocol.cpp
│       ├── poker_rules/
│       │   ├── hand_evaluator.hpp # (Epic 3)
│       │   ├── hand_evaluator.cpp
│       │   ├── poker_rules.hpp
│       │   └── poker_rules.cpp
│       ├── game_engine/
│       │   ├── game_engine.hpp    # (Epic 3)
│       │   ├── game_engine.cpp
│       │   ├── action_validator.hpp # (Epic 4)
│       │   └── action_validator.cpp
│       ├── logging/
│       │   ├── event_bus.hpp      # (Epic 7)
│       │   ├── event_bus.cpp
│       │   ├── server_logger.hpp
│       │   ├── server_logger.cpp
│       │   ├── client_logger.hpp
│       │   └── client_logger.cpp
│       └── utils/
│           └── (utility headers/sources)
├── tests/
│   ├── CMakeLists.txt             # Test build config (from Story 1.1)
│   ├── unit/
│   │   ├── test_hand_evaluator.cpp    # (Epic 9)
│   │   ├── test_poker_rules.cpp
│   │   ├── test_game_engine.cpp
│   │   ├── test_action_validator.cpp
│   │   └── test_protocol.cpp
│   ├── integration/
│   │   ├── test_server_client.cpp     # (Epic 9)
│   │   ├── test_complete_hand.cpp
│   │   └── test_reconnection.cpp
│   └── stress/
│       ├── stress_test_harness.cpp    # (Epic 9)
│       └── uptime_test.cpp
├── cmake/
│   ├── CompilerWarnings.cmake     # (Story 1.3)
│   └── (other CMake helper modules)
└── docs/
    ├── architecture.md            # (Existing)
    ├── prd.md                     # (Existing)
    ├── epics.md                   # (Existing)
    ├── protocol-spec.md           # (Future)
    └── api-reference.md           # (Future)
```

---

## Architecture Compliance

### Monorepo Organization Principles

**Separation of Concerns:**
- **`src/server/`** - Server-only code (WebSocket server, connection management)
- **`src/client/`** - Client-only code (bot strategy, client-side logic)
- **`src/common/`** - Shared code (protocol, poker rules, game engine, logging)

**Component-Based Design:**
The `src/common/` directory is organized by **functional components**:
- `protocol/` - JSON message schemas, serialization
- `poker_rules/` - NLHE game logic, hand evaluation
- `game_engine/` - State machine, game orchestration, validation
- `logging/` - Event bus, server/client loggers
- `utils/` - Utility functions, helpers

**Testing Organization:**
- `unit/` - Unit tests for individual components (poker_rules, hand_evaluator, etc.)
- `integration/` - End-to-end tests (server-client interactions, complete hands)
- `stress/` - Long-running tests (10,000+ hands, 72-hour uptime)

---

## Implementation Notes

### What to Create in This Story

**New Directories:**
```bash
src/common/protocol/
src/common/poker_rules/
src/common/game_engine/
src/common/logging/
src/common/utils/
tests/unit/
tests/integration/
tests/stress/
cmake/
```

**New Files:**
```bash
README.md
.gitignore
```

**Existing from Story 1.1 (Do NOT recreate):**
- `CMakeLists.txt` (root, server, client, common, tests)
- `src/server/main.cpp`
- `src/client/main.cpp`

### Placeholder Headers (Optional for Story 1.2)

You may create **empty placeholder .hpp files** with header guards to visualize the structure:

**Example: `src/common/protocol/protocol.hpp`**
```cpp
#pragma once

// Protocol definitions will be implemented in Epic 2
// This file serves as a placeholder for directory structure
```

**However, this is optional.** Epic 2+ stories will create these files, so you can leave subdirectories empty for now.

---

## File Content Requirements

### README.md

Create a basic project README:

```markdown
# cppsim - C++ Poker Server Simulation

A high-performance, single-threaded WebSocket poker server for heads-up No-Limit Hold'em, implemented in C++17.

## Features

- WebSocket + JSON protocol for client-server communication
- Complete NLHE poker game engine with hand evaluation
- Authoritative server design with comprehensive validation
- Dual-perspective logging for complete hand traceability
- Autonomous bot clients for testing and simulation
- Comprehensive test suite (unit, integration, stress tests)

## Build Instructions

### Prerequisites

- CMake 3.15 or later
- C++17-compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Internet connection (for CMake FetchContent dependency downloads)

### Build Steps

```bash
# Configure
cmake -B build -S .

# Build
cmake --build build

# Run tests
./build/tests/poker_tests

# Run server
./build/src/server/poker_server

# Run client
./build/src/client/poker_client
```

## Project Structure

- `src/server/` - Server executable
- `src/client/` - Bot client executable
- `src/common/` - Shared library (protocol, game logic, logging)
- `tests/` - Unit, integration, and stress tests

## Documentation

- [Architecture](docs/architecture.md)
- [PRD](docs/prd.md)
- [Epics & Stories](docs/epics.md)

## License

[Specify license - e.g., MIT, Apache 2.0, or Proprietary]
```

### .gitignore

Create a comprehensive `.gitignore`:

```gitignore
# Build artifacts
build/
cmake-build-*/

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
install_manifest.txt

# Compiled files
*.o
*.obj
*.a
*.lib
*.so
*.dylib
*.dll
*.exe

# macOS
.DS_Store

# Windows
Thumbs.db
```

---

## Testing Requirements

### Verification Steps

After implementation, verify:

1. **Directory structure exists:**
   ```bash
   ls -la src/server src/client src/common tests/unit tests/integration tests/stress cmake docs
   ```

2. **README.md is readable:**
   ```bash
   cat README.md
   ```

3. **Git ignore works:**
   ```bash
   git status  # Should not show build/ or IDE files
   ```

4. **CMake still builds successfully:**
   ```bash
   cmake -B build -S .
   cmake --build build
   ```

---

## Definition of Done

- [ ] All directories from the structure diagram are created
- [ ] `src/common/` subdirectories created (protocol, poker_rules, game_engine, logging, utils)
- [ ] Test subdirectories created (unit, integration, stress)
- [ ] `cmake/` directory created
- [ ] `README.md` created with build instructions
- [ ] `.gitignore` created with appropriate patterns
- [ ] Directory structure matches architecture specification exactly
- [ ] CMake build still works (Story 1.1 functionality preserved)
- [ ] Git repository tracks new directories correctly

---

## Context & Dependencies

### Depends On
- **Story 1.1:** CMake Project Structure & Dependencies (must be completed first)

### Blocks
- **Story 1.3:** Coding Standards & Style Enforcement
- **All Epic 2+ stories** will populate these directories with actual code

### Related Stories
- **Epic 2+:** Will create actual implementation files in these directories
- **Epic 9:** Will create comprehensive test files in `tests/` subdirectories

---

## Previous Story Intelligence

### Learnings from Story 1.1

**If Story 1.1 is complete**, review its implementation to ensure:
- CMakeLists.txt files are already in place (don't overwrite them)
- Build system is working correctly
- Placeholder main.cpp files exist for server and client

**Common Patterns to Follow:**
- If Story 1.1 used specific naming conventions, maintain consistency
- If Story 1.1 established a particular CMake structure, preserve it
- Check if any directory paths are already hardcoded in CMakeLists.txt

---

## Developer Notes

### Why Separate common/ Subdirectories?

**Modularity:** Each subdirectory represents a distinct functional component with clear responsibilities.

**Dependency Management:** Components can have explicit dependencies:
- `game_engine/` depends on `poker_rules/`
- `server/` depends on `protocol/` and `game_engine/`
- `logging/` is depended on by all components

**Testability:** Each component can be unit tested independently.

### Directory Creation Best Practices

**Use `mkdir -p`** (Linux/macOS) or PowerShell equivalents to create nested directories:
```bash
mkdir -p src/common/protocol
mkdir -p src/common/poker_rules
mkdir -p src/common/game_engine
mkdir -p src/common/logging
mkdir -p src/common/utils
mkdir -p tests/unit tests/integration tests/stress
mkdir -p cmake
```

**Preserve Existing Files:** Do NOT overwrite CMakeLists.txt or main.cpp files from Story 1.1.

### Estimated Complexity

**Low** - Straightforward directory creation and basic documentation. Key considerations:
- Ensure no conflicts with Story 1.1 output
- Verify `.gitignore` patterns work correctly
- Keep README.md accurate and up-to-date

---

## Resources

### Git Ignore Patterns
- GitHub's C++ .gitignore: https://github.com/github/gitignore/blob/main/C%2B%2B.gitignore
- Git ignore documentation: https://git-scm.com/docs/gitignore

### README Best Practices
- Make a README: https://www.makeareadme.com/
- Awesome README: https://github.com/matiassingers/awesome-readme

---

**This story is ready for development. Once complete, proceed to Story 1.3: Coding Standards & Style Enforcement.**
