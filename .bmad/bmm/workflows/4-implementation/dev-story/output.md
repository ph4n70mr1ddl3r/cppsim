✅ **Story 1.1 Implementation Complete**

The story **1-1-cmake-project-structure-dependencies** has been updated and is **Ready for Review**.

**Summary of Changes:**
- **CMake Refactoring:** Created `poker_server_lib` to allow unit tests to link against server code without recompiling sources.
- **Dependency Management:** Verified `project_warnings` and `project_options` are correctly linked (PUBLIC/PRIVATE) for C++17 compliance.
- **Code Improvements:** Extracted hardcoded port 8080 to `DEFAULT_PORT` constant.
- **Documentation:** Updated story file to reflect that `poker_common` is a STATIC library and included all relevant server files in the File List.
- **Verification:**
  - `cmake` configured successfully.
  - `cmake --build` completed without errors.
  - `ctest` passed 26/26 tests (including handshake and protocol tests).

**Files Modified:**
- `docs/sprint-artifacts/1-1-cmake-project-structure-dependencies.md`
- `src/server/CMakeLists.txt`
- `src/common/CMakeLists.txt`
- `src/server/main.cpp`
- `tests/CMakeLists.txt`
- `docs/sprint-artifacts/sprint-status.yaml`

**Next Steps:**
1. Run `code-review` workflow to verify these changes (recommended to use a different LLM/Agent).
2. Proceed to the next story in the sprint.
