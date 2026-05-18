# Code Review #34

**Reviewer:** pi (automated)
**Date:** 2026-05-19
**Scope:** Full codebase (`src/`, `tests/`, `CMakeLists.txt`, build scripts)
**Status:** All 61 tests pass, build clean with -Werror

## Summary

Codebase is mature after 33 prior review iterations. This review focuses on
header dependency hygiene, lifetime safety, and API completeness.

---

## Findings & Changes

### 1. [MEDIUM] Remove heavy `<nlohmann/json.hpp>` include from `websocket_session.hpp`

**File:** `src/server/websocket_session.hpp:15`

The header included the full nlohmann/json header (~14K lines) solely for three
private method signatures that took `const nlohmann::json&` parameters:

```cpp
void handle_action(const nlohmann::json& envelope_json, ...);
void handle_reload_msg(const nlohmann::json& envelope_json, ...);
void handle_disconnect_msg(const nlohmann::json& envelope_json, ...);
```

Since these are private methods only called internally from `on_read`, they
should use the existing `protocol::parsed_message_header` struct instead of
exposing the JSON dependency through the header.

**Fix:** Changed to `const protocol::parsed_message_header&` parameters and
replaced `#include <nlohmann/json.hpp>` with `#include "protocol.hpp"`.
Updated the `.cpp` call sites to dereference `*header_opt`.

### 2. [MEDIUM] Eliminate `shared_from_this()` crash path in `close()`

**File:** `src/server/websocket_session.cpp:close()`

`close()` used `shared_from_this()` to capture `self` in the strand dispatch
lambda. If the session object was being destroyed (no existing `shared_ptr`),
this would throw `std::bad_weak_ptr`, triggering a 30-line fallback cleanup
block that duplicated destructor logic.

**Fix:** Replaced with `weak_from_this()` pattern — the lambda captures a
`weak_ptr`, attempts `lock()` inside, and silently returns if the object is
already being destroyed (the destructor handles cleanup). This eliminates the
entire `bad_weak_ptr` catch block and the duplicated cleanup code.

### 3. [LOW] Add `empty()` convenience method to `connection_manager`

**File:** `src/server/connection_manager.hpp`, `connection_manager.cpp`

The `session_count()` method requires callers to compare against 0. A more
idiomatic `empty()` method improves readability for the common "any sessions?"
check. Added with `[[nodiscard]]` consistent with existing query methods.

---

## No-Action Items (Verified Correct)

- Build scripts already use `set -euo pipefail`
- `validate_clang_format.sh` already exits non-zero on missing clang-format
- All `[[nodiscard]]` attributes are properly applied to return-value functions
- `connection_manager::stop_all()` is naturally idempotent (clear on empty map)
- `double` for monetary amounts is a known architectural decision; documented
  in prior reviews
- `config` struct with `= delete` constructor is a valid C++17 pattern for
  grouping constants
- `player_stack::seat` uses `int` with sentinel `-1` — clear and intentional
- ALL_IN requiring amount is a valid protocol design for server-side validation
- Destructor properly handles cleanup when `do_close()` hasn't been called
- Rate limiting with sliding-window deque is correct
- Session ID CSPRNG with platform-specific backends is robust
- Write pipeline (mutex + strand) is thread-safe
- All `noexcept` boundaries have internal try/catch where needed
- Integration tests use `find_free_port` to avoid port collisions
- No TODO/FIXME/HACK markers in source
- No `using namespace std`, no raw `new`/`delete`, no `reinterpret_cast`
- All headers use `#pragma once` consistently

---

## Changes Made

| File | Change |
|------|--------|
| `src/server/websocket_session.hpp` | Replace `<nlohmann/json.hpp>` with `"protocol.hpp"`, change private handler signatures to take `const protocol::parsed_message_header&` |
| `src/server/websocket_session.cpp` | Update handler implementations and call sites for new signatures |
| `src/server/websocket_session.cpp` | Replace `shared_from_this()` with `weak_from_this()` in `close()`, eliminate `bad_weak_ptr` catch block |
| `src/server/connection_manager.hpp` | Add `[[nodiscard]] bool empty() const noexcept` declaration |
| `src/server/connection_manager.cpp` | Add `empty()` implementation |
| `docs/code-review-9.md` | This review document |
