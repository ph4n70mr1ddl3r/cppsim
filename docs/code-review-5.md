# Code Review #5

**Date:** 2026-05-16
**Scope:** Full codebase review (`src/`, `tests/`, CMake build system, scripts)
**Status:** Completed — recommendations implemented

## Summary

The codebase is in excellent shape after 18 prior review commits. Zero compiler warnings, all 59 tests pass. This review focused on correctness, resource management, code hygiene, and consistency. Six findings were identified and all have been addressed.

## Findings & Actions

### 1. [MEDIUM] Simplify `register_session` retry logic — **FIXED**
- **File:** `src/server/connection_manager.cpp`
- **Issue:** The retry loop used a conditional move-on-last-attempt pattern: `sessions_.try_emplace(session_id, session)` for attempts 0..N-2 but `sessions_.try_emplace(session_id, std::move(session))` on the final attempt. This was intended to "keep the caller's ptr alive for retries" but was misleading — `session` is a local copy of the caller's `shared_ptr`, so moving it only empties the local copy, not the caller's. The caller always retains their reference. The conditional logic added unnecessary complexity.
- **Fix:** Simplified to always copy the `shared_ptr` via `try_emplace`. Added a comment explaining that `session` is a shared_ptr copy so the caller always retains their reference. Added an explicit error log after the retry loop exhausts all attempts.

### 2. [LOW] Add clarifying comment to `check_deadline` operation_aborted path — **FIXED**
- **File:** `src/server/websocket_session.cpp`
- **Issue:** The `check_deadline` timer callback reschedules itself on `operation_aborted` when the session isn't closed and no close has been requested. This is correct but the intent wasn't clearly documented — a reader might wonder why the timer is rescheduled on cancellation.
- **Fix:** Added a comment explaining that `do_close()` or `close()` cancelling the timer should stop the timer chain, not reschedule.

### 3. [MEDIUM] Document strand safety of `current_stack_` compound operation — **FIXED**
- **File:** `src/server/websocket_session.cpp`
- **Issue:** `handle_reload_msg` performs a read-then-write on `current_stack_` (a non-atomic `double`). While this is safe under the current strand-only access model, the compound operation's safety was not documented, making it fragile for future refactors.
- **Fix:** Added a comment explaining that `current_stack_` is only accessed from the session's strand (single-threaded), so the read-then-write is not a data race.

### 4. [LOW] Remove unnecessary `shared_ptr` wrapper around error logger — **FIXED**
- **File:** `src/common/protocol.cpp`
- **Issue:** The global error logger was stored as `shared_ptr<function<void(string_view)>>`, requiring heap allocation and two levels of indirection on every log call. A `std::function` alone suffices — it's already type-erased and internally heap-allocated. The `shared_ptr` wrapper added overhead without benefit since the mutex already protects concurrent swaps.
- **Fix:** Replaced with a plain `std::function<void(std::string_view)>`. Updated `log_protocol_error` and `set_error_logger` accordingly. Removed the now-unnecessary `<memory>` include.

### 5. [MEDIUM] Use `std::random_device` for test port selection — **FIXED**
- **File:** `tests/integration/websocket_server_test.cpp`, `tests/integration/handshake_test.cpp`
- **Issue:** Integration tests used `getpid() % 10000` for port selection, giving deterministic ports per-process. With `gtest_discover_tests` (each test in its own process), this was mostly safe but could still collide with other processes on the system. Replaced `unistd.h` dependency with `<random>`.
- **Fix:** Changed port selection to `30000 + (std::random_device{}() % 20000)`, providing random ports across the 30000–49999 range. Replaced `#include <unistd.h>` with `#include <random>` in both test files.

### 6. [LOW] Fix `.gitignore` entry for `build-review` — **FIXED**
- **File:** `.gitignore`
- **Issue:** `build-review` without a trailing `/` only matches a file named exactly `build-review`, not a directory. If the intent is to ignore a `build-review/` directory, the trailing slash is needed.
- **Fix:** Changed to `build-review/` with a section comment for review artifacts.

## Items Reviewed — No Action Needed

- **Strand correctness:** All async operations on `websocket_session` dispatch through the session's strand. Writes are serialized via `write_queue_mutex_`. Reads are single-threaded via strand. Close is idempotent via `close_initiated_` atomic. Verified correct.
- **Write queue drain on close:** Traced: `close()` → sets `close_requested_` → posts to strand → checks `writing_` → if writing, `on_write` drains → calls `do_close()` when queue empty. Error messages queued before close are always sent. Correct.
- **Timer lifecycle:** `check_deadline()` reschedules on `operation_aborted` only when session is alive and no close requested. `do_close()` cancels the timer and sets state to `closed`. No infinite timer chains possible. Correct.
- **Session ID generation:** CSPRNG (Linux/BSD/Windows) with MurmurHash3-mixed fallback. 128-bit entropy makes collision probability negligible. Three retries with explicit error logging after exhaustion. Correct.
- **`noexcept` boundaries:** All destructor-callable functions have try/catch blocks. No uncaught exceptions can escape. Correct.
- **`[[nodiscard]]` coverage:** All functions whose return values must be checked are properly annotated. Correct.
- **Protocol versioning:** `parse_handshake` intentionally doesn't enforce version (caller's responsibility). Other `parse_*` functions enforce it via the `parse_from_envelope` template. Test coverage exists. Correct.

## Deferred Items

- **`double` for monetary values:** Using `double` for poker chip amounts is a known floating-point concern. Should be addressed in a future sprint (integer cents or decimal library).
- **Client placeholder:** `src/client/main.cpp` is a stub. Expected in future sprint.
- **Header split:** `protocol.hpp` is ~300 lines with inline serialization. Could be split into types + functions, but deferred as low priority.
- **Port 0 binding:** Ideally integration tests would bind to port 0 and retrieve the assigned port from the acceptor, but this requires API changes to expose the bound port from `websocket_server`.
