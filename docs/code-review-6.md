# Code Review #6

**Reviewer:** pi (automated)
**Date:** 2026-05-16
**Scope:** Full codebase (`src/`, `tests/`)
**Status:** All 59 tests pass, build clean with -Werror

## Summary

The codebase is mature and well-hardened after 21 prior review iterations. This review
focuses on correctness issues, missed edge cases, minor robustness improvements, and
test coverage gaps. The findings are ordered by severity.

---

## Findings

### 1. [MEDIUM] `config::MAX_SESSION_ID_LENGTH` static_assert is fragile

**File:** `src/server/config.hpp:29`

```cpp
static_assert(MAX_SESSION_ID_LENGTH == 128, "Must match protocol::MAX_SESSION_ID_LENGTH in protocol.hpp");
```

The comment says "Must match protocol::MAX_SESSION_ID_LENGTH" but the static_assert compares
against the literal `128` rather than `protocol::MAX_SESSION_ID_LENGTH`. If someone changes the
protocol constant, this assert will still pass until the *value* diverges — but the intent is to
link the two declarations. The static_assert is checking the right thing (value equality), but
the coupling should be explicit: `config` should not duplicate the constant at all — it should
`#include "protocol.hpp"` and use `protocol::MAX_SESSION_ID_LENGTH` directly, or the server
should reference the protocol constant.

**Recommendation:** Remove the duplicate constant from `config` and use
`protocol::MAX_SESSION_ID_LENGTH` everywhere in server code. The `config` struct can add a
`static_assert` referencing the protocol constant by value for documentation, but should not
re-declare it.

---

### 2. [MEDIUM] `handle_action` logs action type from untrusted input without truncation

**File:** `src/server/websocket_session.cpp:334`

```cpp
log_message(std::string("[WebSocketSession] Validated ACTION from ") + sid + ": type=" +
            action_opt->action_type + " seq=" + std::to_string(seq));
```

The `action_type` here is already validated against the known set of valid types, so it's
safe — but only because of the validation in `protocol::validate_action`. If that validation
is ever relaxed, this becomes a log injection vector. The same pattern appears elsewhere
with truncated fields (e.g., `trunc_field` in protocol.cpp). For defense-in-depth, truncate
the action_type in the log message.

**Recommendation:** Truncate `action_type` in the log message using the existing
`sanitize_session_id` pattern or a simple `substr(0, 32)`.

---

### 3. [LOW] `on_accept` successful path doesn't reset `backoff_timer_` under timer mutex

**File:** `src/server/websocket_server.cpp:133-136`

```cpp
  // Release the backoff timer if one was created during a previous error
  {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    backoff_timer_.reset();
  }
```

This is correct — the reset IS under the mutex. Good. No action needed. (Verified during
review.)

---

### 4. [MEDIUM] Missing test coverage: `reload_request` with valid session ID but invalid format

The unit tests in `protocol_test.cpp` cover reload amount validation (negative, zero, NaN,
infinity, exceeds max) and disconnect session_id validation, but there's no test for a
reload request with a malformed session_id (e.g., `<script>` tag). The `validate_reload`
function checks session_id format, so this path is code-covered but not test-covered.

**Recommendation:** Add a test for reload request with invalid session_id format.

---

### 5. [LOW] `player_stack::seat` should use a signed type

**File:** `src/common/protocol.hpp:56`

```cpp
struct player_stack {
  int seat;
  double stack;
};
```

`seat` is `int` while `config::PLACEHOLDER_SEAT` is `-1` — this is consistent. Good.

---

### 6. [LOW] `handshake_timeout_` stored as `std::chrono::seconds` but could be any duration

**File:** `src/server/websocket_session.hpp:155`

The constructor accepts `std::chrono::seconds` which limits timeout granularity. If sub-second
timeouts are ever needed, this would need to change. Current usage is fine for the poker
server's 10s timeout, but the type constraint is unnecessarily restrictive.

**Recommendation:** No action — this is fine for the current use case. Noted for future
reference only.

---

### 7. [MEDIUM] `check_rate_limit()` returns `bool` but side-effects (close) on failure

**File:** `src/server/websocket_session.cpp:175-195`

`check_rate_limit()` both returns a boolean AND calls `close()` on failure. This is a
side-effecting predicate, which is a code smell. The caller in `on_read` uses it as:

```cpp
if (!check_rate_limit()) {
    return;
}
```

The function name doesn't suggest it will close the session. This is technically correct
but could confuse future maintainers.

**Recommendation:** Rename to `check_rate_limit_or_close()` to make the side effect visible
at the call site, or have the caller handle the close.

---

### 8. [LOW] `generate_fallback_session_id` uses MurmurHash3 finalizer but produces only 64 bits

**File:** `src/server/connection_manager.cpp:49-65`

The fallback session ID generator mixes entropy sources and produces 64 bits. While the
crypto path generates 128-bit IDs, the fallback is documented as a degraded mode. The
`log_error` call when falling back is appropriate.

**Recommendation:** No action needed — the fallback is documented and logged.

---

### 9. [MEDIUM] Test port collision window between `websocket_server_test.cpp` and `handshake_test.cpp`

**Files:** `tests/integration/websocket_server_test.cpp`, `tests/integration/handshake_test.cpp`

Both test files use `30000 + random_device() % 20000` for port selection. The range is
`[30000, 49999]`. With parallel `ctest --parallel`, these tests can still collide if
two processes pick the same random port. The window is small (1/20000) but non-zero.

A more robust approach is to use `SO_REUSEPORT` or to bind to port 0 and query the
assigned port. Since the server constructor currently requires a port and throws on
bind failure, retrying with a new port would require restructuring.

**Recommendation:** Add a retry loop around server construction in tests that catches
bind failures and retries with a new port. Alternatively, use a port assignment service.

---

### 10. [LOW] `using namespace cppsim::protocol;` in test file

**File:** `tests/unit/protocol_test.cpp:6`

This is a test file, so the `using namespace` directive is acceptable. The project's
production code properly uses explicit namespace qualifications.

**Recommendation:** No action.

---

## Recommendations Implemented

The following changes will be made:

1. **Remove duplicate `MAX_SESSION_ID_LENGTH` from config, reference protocol constant**
2. **Truncate action_type in handle_action log message** (defense in depth)
3. **Rename `check_rate_limit` to `check_rate_limit_or_close`** (clarity)
4. **Add test: reload request with invalid session_id format**
5. **Add port retry logic in integration tests** (reduce flakiness)
