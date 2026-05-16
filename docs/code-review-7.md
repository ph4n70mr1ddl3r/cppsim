# Code Review #23

**Reviewer:** pi (automated)
**Date:** 2026-05-16
**Scope:** Full codebase (`src/`, `tests/`, `CMakeLists.txt`, build scripts)
**Status:** All 60 tests pass, build clean with -Werror

## Summary

Codebase is mature after 22 prior review iterations. This review focuses on crash-safety
edge cases, platform-specific robustness, and developer experience improvements.

---

## Findings

### 1. [HIGH] `on_read` catch-all blocks can crash server on `std::bad_alloc`

**File:** `src/server/websocket_session.cpp`

Both the `catch (const std::exception&)` and `catch (...)` blocks constructed log
messages via `operator+` string concatenation outside any try/catch. If `std::bad_alloc`
fires during concatenation, the exception escapes the catch block and propagates through
the Boost.Asio completion handler, terminating the server.

**Fix:** Wrapped string construction in inner `try/catch` blocks so logging failures
are silently swallowed rather than crashing the process.

### 2. [MEDIUM] Logger `get_timestamp()` didn't handle `strftime` failure

**File:** `src/server/logger.cpp`

`std::strftime` returns 0 on failure (buffer too small or invalid format). The return
value was used directly as an offset for `snprintf` without validation. On failure, the
buffer contents are undefined, producing garbled timestamps.

**Fix:** Added a check for `len == 0` with a safe fallback format `"ts-err.NNN"`.

### 3. [MEDIUM] Windows `get_secure_random()` wasted 75% of `rand_s()` entropy

**File:** `src/server/connection_manager.cpp`

Each `rand_s()` call produces 4 random bytes but the previous code only used 1 byte per
call, discarding the other 3. For the 16-byte session ID, this meant 16 syscalls instead
of 4.

**Fix:** Refactored to use all 4 bytes from each `rand_s()` call, reducing syscalls by 4x
on Windows.

### 4. [LOW] `.gitignore` missing `.cache/` and `compile_commands.json`

Common artifacts from clangd/LSP tooling and CMake's `CMAKE_EXPORT_COMPILE_COMMANDS`
were not ignored.

**Fix:** Added `.cache/` and `compile_commands.json` to `.gitignore`.

### 5. [LOW] `CMakeLists.txt` didn't set `CMAKE_EXPORT_COMPILE_COMMANDS`

Useful for clangd, clang-tidy, and IDE integration. Should default to ON for a better
developer experience.

**Fix:** Added `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` to root `CMakeLists.txt`.

---

## No-Action Items (Verified Correct)

- `config::MAX_SESSION_ID_LENGTH` correctly references `protocol::MAX_SESSION_ID_LENGTH`
  directly (fixed in review #6)
- `check_rate_limit_or_close()` naming correctly communicates side effects (fixed in #6)
- Two-guard pattern in `do_close()` prevents double unregister/crash
- `get_session_id_safe()` is `noexcept` with internal try/catch
- Strand dispatch in `do_read()` prevents concurrent reads
- Session lifecycle managed correctly through shared_ptr and connection_manager
- All `constexpr const char*` declarations are redundant but harmless (cosmetic only)

---

## Changes Made

| File | Change |
|------|--------|
| `src/server/websocket_session.cpp` | Wrap catch-all log string construction in try/catch |
| `src/server/logger.cpp` | Handle `strftime` failure with fallback format |
| `src/server/connection_manager.cpp` | Optimize Windows `rand_s()` to use all 4 bytes |
| `.gitignore` | Add `.cache/`, `compile_commands.json` |
| `CMakeLists.txt` | Add `CMAKE_EXPORT_COMPILE_COMMANDS ON` |
