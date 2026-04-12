# Code Review 2 — Full Codebase

**Date:** 2026-04-13
**Scope:** Entire codebase (`src/`, `tests/`, `cmake/`, root build files)
**Result:** ✅ All issues addressed

---

## Issues Found & Resolved

### 1. [Medium] `websocket_server::stop()` not idempotent — duplicate log noise

**File:** `src/server/websocket_server.cpp`
**Problem:** Calling `stop()` twice (common: once explicitly, once from destructor) produced duplicate "All sessions stopped" and spurious "Accept failed: Operation canceled" errors. Tests showed 2× "All sessions stopped" and 2× "Stopped accepting connections" on teardown.
**Fix:** Added `std::atomic<bool> stopped_` flag with `compare_exchange_strong` guard. Second and subsequent calls are silent no-ops.
**Test:** Added `WebSocketServerTest.StopIsIdempotent` — calls `stop()` 3× and verifies single clean shutdown.

### 2. [Medium] Handshake timeout test excluded from CI — critical path untested

**File:** `tests/integration/handshake_test.cpp`
**Problem:** `HandshakeTest.HandshakeTimeout` required a 10-second wait (`config::HANDSHAKE_TIMEOUT`), making it impractical for CI. The test was excluded via `--gtest_filter="-*Timeout*"`, meaning the server's timeout cleanup path was never validated.
**Fix:**
- Made handshake timeout configurable: `websocket_session` and `websocket_server` now accept a `std::chrono::seconds handshake_timeout` parameter (defaults to `config::HANDSHAKE_TIMEOUT`).
- Tests use a 1-second timeout via `TEST_HANDSHAKE_TIMEOUT`.
- Test now runs in ~1 second and validates the full timeout → close → cleanup path.

### 3. [Medium] Duplicate anonymous namespaces in `connection_manager.cpp`

**File:** `src/server/connection_manager.cpp`
**Problem:** Two separate anonymous namespaces — one for crypto/session ID helpers at file scope, another for `sanitize_session_id` inside `namespace cppsim::server`. This caused confusion about which symbols had internal linkage and split related utilities across two blocks.
**Fix:** Consolidated into a single anonymous namespace at file scope before `namespace cppsim::server`.

### 4. [Low] Non-portable format specifier for session ID

**File:** `src/server/connection_manager.cpp`
**Problem:** `generate_fallback_session_id()` used `%016llx` with `static_cast<unsigned long long>` — technically non-portable (though works on all common platforms).
**Fix:** Changed to `PRIx64` from `<cinttypes>` for standards-compliant `uint64_t` formatting. Added `#include <cinttypes>`.

### 5. [Low] `current_stack_` lacking thread-safety documentation

**File:** `src/server/websocket_session.hpp`
**Problem:** `current_stack_` is a plain `double` with no documentation about thread safety. While safe (all access is serialized on the session's strand), this was not obvious to readers.
**Fix:** Added comment: "Mutable by handlers running on the session's strand only — no concurrent access."

### 6. [Low] Missing `connection_manager` unit test coverage

**File:** `tests/integration/websocket_server_test.cpp`
**Problem:** Only `BasicLifecycle` (construction) was tested. Edge cases like `stop_all()` on empty map, `unregister_session` with unknown ID, and `get_session` miss were untested.
**Fix:** Added 3 new tests:
- `ConnectionManagerTest.StopAllOnEmpty` — verifies no crash on empty stop
- `ConnectionManagerTest.UnregisterUnknownSession` — verifies graceful handling
- `ConnectionManagerTest.GetNonexistentSession` — verifies returns nullptr

### 7. [Low] Duplicated server readiness check code in tests

**File:** `tests/integration/websocket_server_test.cpp`, `tests/integration/handshake_test.cpp`
**Problem:** Both test files had copy-pasted retry loops to wait for server readiness (TCP probe with sleep loop).
**Fix:** Extracted `wait_for_server()` helper function in both test files. Used `ASSERT_TRUE` instead of `FAIL()` for clearer failure messages.

---

## Architectural Observations (No Changes Needed)

### Well-Designed Patterns
- **Strand-based concurrency:** All session state mutations run on the WebSocket stream's strand, eliminating data races without heavy locking.
- **CSPRNG session IDs:** Crypto-quality random IDs with fallback to entropy-mixed hashes. Session ID format validation prevents injection attacks.
- **Write queue with close-drain:** The `close()` → drain queue → `do_close()` pattern ensures protocol error messages are delivered before the connection closes.
- **Rate limiting with sliding window:** Uses `std::deque` of timestamps with `std::remove_if` for efficient window cleanup.
- **Exponential backoff on accept failure:** Prevents tight error loops on transient failures, capped at `config::MAX_BACKOFF`.
- **Protocol envelope pattern:** Clean separation of transport (message_type, protocol_version) from payload, with version negotiation.
- **Extensive input validation:** `validate_action`, `validate_reload`, `validate_disconnect` provide defense-in-depth against malformed messages.

### Known Technical Debt
1. **Probe connection creates ghost accepts:** Server readiness checks create TCP connections that the server tries to WebSocket-accept, generating `[ERROR] Accept failed: The WebSocket stream was gracefully closed at both endpoints`. This is cosmetic only — the session is immediately cleaned up. A proper fix would require a dedicated health-check endpoint or out-of-band readiness signal.
2. **`std::atomic_load` on `shared_ptr`:** `protocol.cpp` uses the C++17 deprecated `std::atomic_load(&error_logger)`. This works but should migrate to `std::atomic<std::shared_ptr<>>` when moving to C++20.
3. **Inline JSON serialization in header:** All `to_json`/`from_json` overloads in `protocol.hpp` are inline. This is the standard pattern for nlohmann/json ADL, but means every TU that includes the header gets a copy. Acceptable for this project size.

---

## Files Modified

| File | Changes |
|------|---------|
| `src/server/websocket_server.hpp` | Added `handshake_timeout_` member, `stopped_` guard, configurable constructor |
| `src/server/websocket_server.cpp` | Idempotent `stop()`, pass handshake timeout to sessions |
| `src/server/websocket_session.hpp` | Added `handshake_timeout` constructor param, documented `current_stack_` |
| `src/server/websocket_session.cpp` | Use configurable `handshake_timeout_` instead of config constant |
| `src/server/connection_manager.cpp` | Consolidated anonymous namespaces, portable `PRIx64` format |
| `tests/integration/websocket_server_test.cpp` | Added 4 connection_manager tests, `StopIsIdempotent` test, `wait_for_server` helper |
| `tests/integration/handshake_test.cpp` | Configurable timeout (1s), extracted `wait_for_server`, timeout test now runs |

## Test Results

```
[==========] 59 tests from 4 test suites ran. (1067 ms total)
[  PASSED  ] 59 tests.
```

- **Before:** 54 tests (timeout test excluded)
- **After:** 59 tests (all pass, including timeout test in ~1s)
