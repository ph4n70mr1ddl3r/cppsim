# Code Review #3

**Date:** 2026-04-28
**Scope:** Full codebase review (`src/`, `tests/`, CMake build system)
**Status:** Completed — recommendations implemented

## Summary

The codebase is well-structured with strong security practices (CSPRNG session IDs, input validation, rate limiting) and good test coverage. This review focused on deprecated API usage, missing includes, code duplication, and consistency issues.

## Findings & Actions

### 1. [HIGH] Deprecated `std::atomic_load/store` for `shared_ptr` — **FIXED**
- **File:** `src/common/protocol.cpp`
- **Issue:** `std::atomic_load(&error_logger)` and `std::atomic_store(&error_logger, ...)` are deprecated in C++17 and removed in C++20. This blocks upgrading the language standard.
- **Fix:** Replaced with a `std::mutex`-protected `std::shared_ptr`. The error logger is a low-contention resource (set once at startup, read on parse errors), so a mutex has negligible performance impact compared to the atomic shared_ptr operations it replaces.

### 2. [MEDIUM] Missing `#include <atomic>` — **FIXED**
- **File:** `src/server/connection_manager.cpp`
- **Issue:** Uses `std::atomic<uint64_t>` in `generate_fallback_session_id()` but doesn't include `<atomic>` directly. Relies on `<thread>` transitively including it, which is not guaranteed by the standard and is non-portable.
- **Fix:** Added `#include <atomic>`.

### 3. [MEDIUM] Duplicate sanitize functions — **FIXED**
- **Files:** `src/server/connection_manager.cpp`, `src/server/websocket_session.cpp`
- **Issue:** Two near-identical functions (`sanitize_session_id` and `sanitize_sid`) with inconsistent parameter types (`std::string_view` vs `const std::string&`). These could diverge over time.
- **Fix:** Extracted to `src/server/sanitize.hpp` as a single `inline` function using `std::string_view`. Both translation units now use the shared implementation.

### 4. [MEDIUM] Duplicate `wait_for_server` test helper — **FIXED**
- **Files:** `tests/integration/handshake_test.cpp`, `tests/integration/websocket_server_test.cpp`
- **Issue:** Identical 15-line `wait_for_server()` function duplicated across two test files.
- **Fix:** Extracted to `tests/test_utils.hpp` in the `cppsim::testing` namespace. Updated `tests/CMakeLists.txt` to include the tests directory in the include path.

## Items Reviewed — No Action Needed

These aspects were reviewed and found to be correct:

- **Thread safety:** All shared state is properly protected with mutexes or atomics. Strand-based serialization in websocket_session is correct.
- **`[[nodiscard]]` coverage:** All functions whose return values must be checked are already annotated.
- **Resource management:** RAII patterns used consistently. Shared pointers prevent use-after-free. Timers cancelled before destruction.
- **Error handling:** Comprehensive try/catch in all noexcept boundaries prevents server crashes from unexpected exceptions.
- **Security:** CSPRNG session IDs, rate limiting, input validation, sanitized logging, session ID format validation all present and correct.
- **Build system:** CMake dependency chain is correct. FetchContent properly declared. Warning flags are comprehensive.
- **Protocol layer:** Proper envelope wrapping, version checking, field validation (amounts, session IDs, sequence numbers).

## Deferred Items

- **`double` for monetary values:** Using `double` for poker chip amounts is a known floating-point concern. This is a design-level decision that should be addressed in a future sprint (e.g., using integer cents or a decimal library).
- **Client placeholder:** `src/client/main.cpp` is a stub. Expected to be implemented in a future sprint.
- **Windows CPRNG:** `get_secure_random()` has no Windows path. Acceptable for current Linux-first scope but should be addressed for cross-platform support.
