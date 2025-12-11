# Story 2.3: Handshake & Protocol Versioning

**Status:** Ready for Review
**Epic:** Epic 2 - Core Protocol & Network Communication
**Story ID:** 2.3
**Estimated Effort:** Small (4-8 hours)

---

## User Story

As a **client**,
I want **to perform a handshake with protocol version negotiation**,
So that **the server validates my client version**.

---

## Acceptance Criteria

**Given** a client connects to the server
**When** the client sends a HANDSHAKE message with protocol version
**Then** the server validates the protocol version
**And** responds with session assignment if version is compatible
**And** rejects connection with error message if version is incompatible
**And** the client receives confirmation and session ID

### Detailed Scenarios

#### Scenario 1: Successful Handshake
- **Given** server expects version "v1.0"
- **When** client sends `{"message_type": "HANDSHAKE", "protocol_version": "v1.0", "payload": {"protocol_version": "v1.0"}}`
- **Then** server responds with `{"message_type": "HANDSHAKE_RESPONSE", ...}`
- **And** connection remains open and authenticated
- **And** server logs "Session authenticated: session_X"

#### Scenario 2: Incompatible Version
- **Given** server expects version "v1.0"
- **When** client sends `{"message_type": "HANDSHAKE", "protocol_version": "v0.9", ...}`
- **Then** server responds with `{"message_type": "ERROR", "payload": {"error_code": "INCOMPATIBLE_VERSION", ...}}`
- **And** server closes the WebSocket connection
- **And** server logs "Handshake rejected: Incompatible version v0.9"

#### Scenario 3: Malformed Handshake
- **Given** a new connection
- **When** client sends invalid JSON or non-handshake message as first message
- **Then** server closes connection immediately
- **And** logs "Handshake failed: Malformed message"
- **Error Code:** `MALFORMED_HANDSHAKE` or `PROTOCOL_ERROR`

#### Scenario 4: Handshake Timeout
- **Given** a new connection is established
- **When** the client sends *nothing* for 10 seconds
- **Then** server closes the connection automatically
- **And** logs "Handshake timeout"

---

## Technical Requirements

### 1. WebSocket Session State Machine

**File:** `src/server/websocket_session.hpp`

- Introduce session states: `UNAUTHENTICATED`, `AUTHENTICATED`, `CLOSED`
- Enforce that the **first** message received must be a `HANDSHAKE`
- Reorganized `on_read` logic:
  1. Parse `message_envelope`
  2. If state is `UNAUTHENTICATED`:
     - Validate type is `HANDSHAKE`
     - Parse `handshake_message`
     - Validate `protocol_version` matches `protocol::PROTOCOL_VERSION`
     - If valid: Register session, send `HANDSHAKE_RESPONSE`, transition to `AUTHENTICATED`
     - If invalid: Send `ERROR`, close connection
  3. If state is `AUTHENTICATED`:
     - Allow other message types (implementation in future stories)
     - Log "Message received" placeholder for now

### 2. Protocol Integration

**File:** `src/server/websocket_session.cpp`

- Use `src/common/protocol.hpp` for all message definitions (already exists)
- Use `protocol::parse_handshake` and `protocol::serialize_handshake_response`
- Ensure JSON parsing errors are caught and handled gracefully (send ERROR or separate handling) (See **Error Handling** below)

### 3. Integration Tests

**File:** `tests/integration/handshake_test.cpp` (New file)

- Test 1: Connect and send valid handshake -> Expect handshake response
- Test 2: Connect and send invalid version -> Expect error and disconnection
- Test 3: Connect and send garbage data -> Expect disconnection

---

## Tasks/Subtasks

### Original Tasks (Reconstructed)
- [x] Implement WebSocket session state machine `src/server/websocket_session.hpp`
- [x] Implement handshake protocol logic `src/server/websocket_session.cpp`
- [x] Create integration tests `tests/integration/handshake_test.cpp`

### Review Follow-ups (AI)
- [x] [AI-Review][Medium] Update File List in `Dev Agent Record` with all implemented files
- [x] [AI-Review][Medium] Implement missing HandshakeTimeout test case (Scenario 4) `tests/integration/handshake_test.cpp`
- [x] [AI-Review][Critical] Implement session registration with connection manager `src/server/websocket_session.cpp`
- [x] [AI-Review][Critical] Fix missing session ID in HANDSHAKE_RESPONSE by retrieving it after registration
- [x] [AI-Review][Low] Use defined constants for error codes in `src/server/websocket_session.cpp`
- [x] [AI-Review][Low] Implement thread-safe logging or mutex for console output

### Review Follow-ups (AI) - Round 2
- [x] [AI-Review][Medium] Hardcoded Version Strings in Tests `tests/unit/protocol_test.cpp`
- [x] [AI-Review][Medium] Protocol Version Ambiguity (Envelope vs Payload) `src/common/protocol.cpp`
- [x] [AI-Review][Low] No Keep-Alive/Idle Timeout `src/server/websocket_session.cpp`
- [x] [AI-Review][Low] Unused Client Name `src/server/websocket_session.cpp`

### Review Follow-ups (AI) - Round 3
- [x] [AI-Review][Critical] Fix Memory Leak: Circular dependency between websocket_session and connection_manager `src/server/websocket_session.hpp`
- [x] [AI-Review][Medium] Refactor `tests/integration/handshake_test.cpp` to use protocol constants
- [x] [AI-Review][Low] Update misleading comment in `protocol.hpp` (global namespace vs ADL)

### Review Follow-ups (AI) - Round 4
- [x] [AI-Review][Low] Unused Member Variable: `client_name_` is set but never used in `websocket_session` `src/server/websocket_session.hpp`

### Review Follow-ups (AI) - Round 5
- [x] [AI-Review][High] Security: Set `max_message_size` in `websocket_session` to prevent memory exhaustion (DoS) `src/server/websocket_session.cpp`
- [x] [AI-Review][Medium] Error Handling: Catch `std::exception` instead of `...` in `protocol.cpp` to avoid swallowing critical errors `src/common/protocol.cpp`
- [x] [AI-Review][Medium] Protocol Logic: Reject (or explicitly validate) handshake if envelope version differs from payload version `src/common/protocol.cpp`
- [x] [AI-Review][Low] Test Quality: Use dynamic port or configuration for `handshake_test.cpp` instead of hardcoded 8080 `tests/integration/handshake_test.cpp`

---

## Dev Notes

### Architecture Compliance

- **Naming:** Continue using `snake_case` for all variables and functions.
- **Error Handling:** Use `std::optional` checks from `protocol::parse_...` functions. Do NOT throw exceptions for parsing failures.
- **Logging:** Use `std::cout` for successful handshake events, `std::cerr` for failures.
- **Protocol:** Strictly adhere to `protocol::PROTOCOL_VERSION` constant. Do not hardcode "v1.0".

### Dependencies

- **Boost.Beast / Asio:** Continue using for WebSocket/Async operations.
- **nlohmann/json:** For parsing/serializing.
- **Common Library:** Link against `cppsim_common` for protocol definitions.

### Project Structure Notes

- `src/server/websocket_session.cpp` will contain the bulk of the logic changes.
- `tests/integration/` is the correct place for the new test file.

### References

- Protocol Definitions: `src/common/protocol.hpp` (defines `handshake_message`, `PROTOCOL_VERSION`)
- Server Architecture: `docs/architecture.md` (Section: Network Protocol Design)

---

## Dev Agent Record

### Context Reference

This story implements the first logical step of the custom protocol over the established WebSocket connection. It transforms the "dumb" echo/log server into a protocol-aware server.

### Agent Model Used

Gemini 2.0 Flash

### Debug Log References

### Completion Notes List
- Implemented `HandshakeTimeout` test in `tests/integration/handshake_test.cpp`.
- Verified all Handshake scenarios pass (Success, Incompatible, Malformed, ProtocolError, Timeout).
- Address all review follow-ups.
- Added AI Review action items for session registration and thread safety.
- Implemented session registration, error codes, and thread-safe logging (Review Follow-ups).
- **Review Fixes**: Implemented central `protocol.cpp` parsing helpers, refactored `websocket_session.cpp` to use them + properly defined protocol constants.
- **Code Review Fix**: Added `tests/unit/protocol_test.cpp` to File List.
- **Round 2 & 3 Fixes**: Fixed circular dependency memory leak, replaced hardcoded strings with constants in tests, clarified protocol ambiguity, and stored client name.
- **Round 4 Fixes**: Removed unused member variable client_name_ and its getter.
- **Round 5 Fixes**: Verified max_message_size (64KB) already set, improved error handling with specific exception types in protocol.cpp, confirmed protocol version validation working, made test port configurable via config::DEFAULT_TEST_PORT.

### File List
- src/common/protocol.hpp
- src/common/protocol.cpp
- src/server/websocket_session.hpp
- src/server/websocket_session.cpp
- src/server/config.hpp
- tests/integration/handshake_test.cpp
- tests/unit/protocol_test.cpp

### Change Log
- 2025-12-11: Implemented initial handshake protocol and tests.
- 2025-12-11: Fixed critical issues from first AI review.
- 2025-12-11: Second AI Review (Adversarial) - Added action items for code quality improvements. Status reverted to In Progress.
- 2025-12-11: Third AI Review (Adversarial) - Added critical memory leak fix and test refactoring to action items.
- 2025-12-11: Implemented Round 2 and Round 3 review fixes (Memory leak, constants, client name).
- 2025-12-11: Implemented Round 4 review fix (Cleanup unused member).
- 2025-12-11: Implemented Round 5 review fixes (Error handling, test port configuration). All 26 tests passing.

