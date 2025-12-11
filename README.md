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
- clang-format 10+ (for code style enforcement)
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

## Platform Notes

**Windows:** Use backslashes or CMake's platform-agnostic build commands:
```cmd
cmake --build build --target poker_server
ctest --test-dir build
```

**Linux/macOS:** Use forward slashes as shown in build instructions above.
