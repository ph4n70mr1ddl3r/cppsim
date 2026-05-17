# Code Review #24

**Reviewer:** pi (automated)
**Date:** 2026-05-17
**Scope:** Full codebase (`src/`, `tests/`, `CMakeLists.txt`, build scripts)
**Status:** All 60 tests pass, build clean with -Werror

## Summary

Codebase is mature after 23 prior review iterations. This review focuses on
performance anti-patterns, dead code, and minor improvements.

---

## Findings

### 1. [MEDIUM] `return std::move(result->message_type)` prevents copy elision

**File:** `src/common/protocol.cpp:101`

`std::move` on a return value inhibits NRVO/copy elision per C++ standard
[class.copy.elision]. The compiler may still optimize it, but the idiomatic
form is to return the expression directly.

**Fix:** Changed `return std::move(result->message_type)` to `return result->message_type`

### 2. [MEDIUM] Unnecessary `std::string()` conversion of `string_view` in log messages

**File:** `src/common/protocol.cpp:154,163,178`

`std::string("[Protocol] ") + std::string(message_name) + " ..."` — the explicit
`std::string(message_name)` conversion allocates a temporary. By restructuring to
`"[Protocol] " + std::string(message_name)`, the `const char*` literal is implicitly
converted to `std::string` first, then the concatenation proceeds without the extra
temporary overhead from wrapping a `string_view` parameter.

**Fix:** Restructured string concatenation to avoid redundant `std::string()` wrapping.

### 3. [LOW] Dead `substr(0, 32)` truncation in action_type log

**File:** `src/server/websocket_session.cpp:357`

`action_opt->action_type.substr(0, 32)` — the action_type has already been validated
against the known set of five short strings ("FOLD", "CHECK", "CALL", "RAISE",
"ALL_IN") by `validate_action()` in the protocol layer. All are ≤ 6 characters.
The substr call was unreachable dead code with a magic number.

**Fix:** Removed the substr truncation entirely — log the full action_type.

### 4. [LOW] Unused `<utility>` include in `protocol.hpp`

**File:** `src/common/protocol.hpp:5`

`<utility>` was included but unused in the header. No `std::pair`, `std::move`,
`std::forward`, or `std::swap` are used. Leftover from earlier iterations.

**Fix:** Removed the unused `#include <utility>`.

---

## No-Action Items (Verified Correct)

- `constexpr const char*` in protocol.hpp is correct spelling: `constexpr` makes the
  pointer a const expression, `const char*` means pointer-to-const-char. Not redundant.
- Two-guard pattern in `do_close()` prevents double unregister
- `check_deadline()` timer chain properly stops on `close_requested_`
- `send()` / `queue_message()` write pipeline is thread-safe with mutex + strand
- Session ID CSPRNG with platform-specific backends is robust
- Rate limiting with sliding window deque is correct
- All `noexcept` boundaries have internal try/catch where needed
- Strand dispatch in `do_read()` prevents concurrent reads
- `validate_action` by-value-taking + returning pattern is correct for move semantics
- `connection_manager::stop_all()` copies sessions before clearing to avoid invalidation
- `.clang-format` and `.clang-tidy` configurations are well-tuned
- `boost_wrapper.hpp` properly suppresses warnings from Boost headers
- `config` struct with `= delete` constructor prevents instantiation
- Integration tests use `find_free_port` to avoid port collisions
- Test cleanup guards properly stop server and join threads
- No TODO/FIXME/HACK markers found in source
- No `using namespace std` found
- No raw `new`/`delete`/`malloc`/`free` found
- No `reinterpret_cast`/`const_cast` found
- All headers use `#pragma once` consistently

---

## Changes Made

| File | Change |
|------|--------|
| `src/common/protocol.cpp` | Remove `std::move` from return of local value (enable copy elision) |
| `src/common/protocol.cpp` | Restructure string concatenation to avoid redundant `std::string()` temporaries |
| `src/server/websocket_session.cpp` | Remove dead `substr(0, 32)` truncation on validated action_type |
| `src/common/protocol.hpp` | Remove unused `#include <utility>` |
| `docs/code-review-8.md` | This review document |
