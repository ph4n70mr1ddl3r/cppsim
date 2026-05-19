# Code Review #10

**Date:** 2026-05-19  
**Reviewer:** pi (automated)  
**Scope:** Full codebase — all source, test, and build files

## Summary

The codebase is in excellent shape after 40 prior reviews. This review found
one category of latent bug (noexcept/throw) across three locations and one
include hygiene issue.

## Findings

### Bug: `std::terminate` risk in `noexcept` functions (Fixed)

Three `noexcept` functions perform heap-allocating string concatenation
*outside* of try/catch blocks. If `std::bad_alloc` is thrown during string
construction, the `noexcept` specification causes an immediate call to
`std::terminate`, crashing the process.

This is a low-probability but high-severity bug — it would only trigger under
memory pressure, making it difficult to reproduce in production.

| Location | Function | String Operation |
|---|---|---|
| `src/common/protocol.cpp` | `validate_session_id_field` | `"[Protocol] Invalid session_id in " + std::string(message_name) + " message"` |
| `src/server/connection_manager.cpp` | `unregister_session` | `sanitize_session_id(session_id) + " (remaining: " + std::to_string(count) + ")"` |
| `src/server/websocket_server.cpp` | `stop()` | `std::string("...") + ec.message()` |

**Fix:** Wrapped each string construction in a try/catch block. The logging
is best-effort — the core operation (session unregistration, server shutdown)
still completes correctly even if the log message is lost.

### Robustness: Missing `#include <string>` in `test_utils.hpp` (Fixed)

`test_utils.hpp` uses `std::to_string()` but does not directly include
`<string>`. It currently works via transitive includes from other headers, but
this violates the include-what-you-use principle and could break if those
headers change their dependencies.

**Fix:** Added `#include <string>`.

### Positive Observations

- **Thread safety model** is well-documented in `websocket_session.hpp` with
  clear categorization of strand-only, mutex-protected, and atomic state.
- **`weak_from_this()` in `close()`** prevents `bad_weak_ptr` crashes when the
  session is being destroyed.
- **Double-guard pattern in `do_close()`** (CAS on `close_initiated_` + state
  exchange) prevents double-close from concurrent callers.
- **Session ID generation** uses proper CSPRNG (`/dev/urandom`, `arc4random`,
  or `rand_s`) with a MurmurHash-mixed fallback.
- **Rate limiting** uses a sliding window with proper mutex protection.
- **Exponential backoff** on accept failure captures `weak_from_this()` and
  validates the timer identity to prevent stale timer callbacks.
- **Protocol validation** is thorough — covers NaN/infinity amounts, printable
  ASCII checks, session ID format validation, and sequence number gaps.
- **Write queue management** properly handles allocation failure by closing the
  session (forcing clean reconnect) rather than silently dropping messages.
- **`log_protocol_error`** is already `noexcept`-safe with its own try/catch —
  the bug was in callers constructing strings *before* calling it.

### Suggestions for Future Reviews

1. **Integration tests for authenticated message flows** — There are no
   integration tests covering ACTION, RELOAD_REQUEST, or DISCONNECT messages
   through the WebSocket server. The protocol parsing is well-tested at the
   unit level, but session-level validation (session ID matching, sequence
   number validation, stack updates) only has unit coverage.

2. **`protocol.hpp` header bloat** — All `to_json`/`from_json` inline functions
   live in the header. Consider splitting into `protocol.inl` or a separate
   `protocol_serialization.hpp` to reduce compilation time for translation
   units that only need the message type definitions.

3. **Test log noise** — Integration tests produce server log output on
   stdout/stderr. Consider adding a `set_log_level()` or log suppression
   mechanism for test builds to reduce noise in test output.
