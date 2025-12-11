# Story 2.3: Handshake & Protocol Versioning

**Status:** ready-for-dev
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

### File List

