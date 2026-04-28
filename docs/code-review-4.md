# Code Review #4

**Date:** 2026-04-28
**Scope:** Full codebase review (`src/`, `tests/`, CMake build system, scripts)
**Status:** Completed — recommendations implemented

## Summary

The codebase continues to mature well. Previous reviews addressed deprecated APIs, missing includes, and utility extraction. This review focused on code correctness, resource management, build robustness, and code hygiene. The codebase demonstrates strong practices: RAII resource management, proper strand-based concurrency, comprehensive input validation, and idempotent cleanup paths. All 69 tests pass cleanly with zero compiler warnings.

## Findings & Actions

### 1. [HIGH] Shared-pointer reference cycle in accept error backoff — **FIXED**
- **File:** `src/server/websocket_server.cpp`
- **Issue:** In `on_accept()`, the exponential backoff timer captures `shared_from_this()` in the async_wait lambda. The lambda is owned by the timer, and the timer is owned by `backoff_timer_`, which is a member of the `websocket_server`. This creates a reference cycle: server → timer → lambda → server. While `stop()` breaks the cycle by cancelling and resetting the timer, if `stop()` is never called (e.g., the server crashes or the `io_context` is destroyed without stopping), the server object leaks.
- **Fix:** Replaced `shared_ptr<websocket_server>` capture with `weak_ptr` in the backoff timer lambda. The lambda now checks if the server is still alive before retrying, naturally breaking the cycle without relying on `stop()` being called. The server can be destroyed as soon as the last external `shared_ptr` is dropped.

### 2. [MEDIUM] Unused includes in `connection_manager.cpp` — **FIXED**
- **File:** `src/server/connection_manager.cpp`
- **Issue:** `<cstring>` and `<limits>` are included but not used. These are remnants from earlier refactoring (the fallback session ID generation was revised in review #3).
- **Fix:** Removed the unused includes.

### 3. [MEDIUM] `test_build.sh` lacks error handling — **FIXED**
- **File:** `test_build.sh`
- **Issue:** The build script has no `set -euo pipefail`, meaning if `cmake` configure fails, the script continues to the build step, producing confusing error output. Additionally, it doesn't specify a build type.
- **Fix:** Added `set -euo pipefail` and `-DCMAKE_BUILD_TYPE=Debug` for consistent debug builds with assertions and sanitizers available.

### 4. [MEDIUM] Missing `<algorithm>` include in `websocket_session.cpp` — **ALREADY PRESENT**
- **File:** `src/server/websocket_session.cpp`
- **Issue:** Initially flagged as potentially missing `<algorithm>` for `std::remove_if` usage.
- **Resolution:** On closer inspection, `#include <algorithm>` is already correctly included. No change needed.

### 5. [LOW] Internal helpers missing `noexcept` — **FIXED**
- **File:** `src/common/protocol.cpp`
- **Issue:** Several internal helper functions in the anonymous namespace (`trunc_field`, `validate_session_id_format`) never throw but aren't marked `noexcept`. While not a correctness issue, explicit `noexcept` enables compiler optimizations and documents intent.
- **Fix:** Added `noexcept` to all internal helpers that cannot throw.

### 6. [LOW] `validate_clang_format.sh` grep safety — **ALREADY CORRECT**
- **File:** `scripts/validate_clang_format.sh`
- **Issue:** Initially flagged as potentially missing `|| true` on grep calls.
- **Resolution:** On inspection, all grep calls already have `|| true` where needed. No change needed.

### 7. [MEDIUM] Doc comment on `writing_` invariant is stale — **FIXED**
- **File:** `src/server/websocket_session.hpp`
- **Issue:** The comment on `writing_` says "INVARIANT: all accesses must hold write_queue_mutex_" but `on_write()` sets `writing_ = false` in a block that also reads `close_requested_` via `load()` on an atomic — the wording could be clearer about the atomic operations.
- **Fix:** Updated the comment to clarify that `writing_` is protected by `write_queue_mutex_` while `close_requested_` is accessed atomically (no mutex needed).

## Items Reviewed — No Action Needed

- **Strand correctness:** All async operations on `websocket_session` dispatch through `ws_.get_executor()` (the strand). Writes are serialized. Reads are single-threaded via strand. Close is idempotent. Verified correct.
- **Write queue drain on close:** Traced the close path: `close()` → sets `close_requested_` → posts lambda to strand → lambda checks `writing_` → if writing, `on_write()` handles drain → `on_write()` calls `do_close()` when queue empty. Queued error messages are always sent before WebSocket close frame. Correct.
- **Timer lifecycle:** `check_deadline()` reschedules on `operation_aborted` only if session is still alive, preventing infinite timer chains. Verified correct.
- **`current_stack_` strand-only access:** Documented as strand-only. Only accessed from handlers dispatched on the session's strand. Safe for single-io_context architecture.
- **Connection manager thread safety:** All mutable operations lock `sessions_mutex_`. `std::map` with `std::less<>` enables transparent `string_view` lookup without temporary string construction. Correct.
- **Protocol versioning:** `parse_handshake` intentionally doesn't enforce `PROTOCOL_VERSION` matching (caller's responsibility). `parse_message` template enforces it. This is a documented design choice with test coverage.
- **Session ID generation:** CSPRNG on Linux/BSD, hash-mixed fallback otherwise. 128-bit entropy makes collision probability negligible. `try_emplace` detects the astronomically unlikely collision with 3 retries. Correct.
- **`noexcept` boundaries:** All functions that can be called from destructors or noexcept contexts have try/catch blocks. No uncaught exceptions can escape.
- **`[[nodiscard]]` coverage:** All functions whose return values must be checked (`parse_*`, `send`, `queue_message`, `validate_session_id`, `wait_for_server`) are properly annotated.

## Deferred Items

- **`double` for monetary values:** Using `double` for poker chip amounts is a known floating-point concern. Should be addressed in a future sprint (integer cents or decimal library).
- **Client placeholder:** `src/client/main.cpp` is a stub. Expected in future sprint.
- **Windows CPRNG:** `get_secure_random()` has no Windows path. Acceptable for current Linux-first scope.
- **Header split:** `protocol.hpp` is ~300 lines with inline serialization. Could be split into types + functions, but deferred as low priority.
