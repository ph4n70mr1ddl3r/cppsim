---
stepsCompleted: ['step-01-validate-prerequisites', 'step-02-design-epics', 'step-03-create-stories', 'step-04-final-validation']
inputDocuments:
  - '\\wsl.localhost\riddler\home\riddler\cppsim\docs\prd.md'
  - '\\wsl.localhost\riddler\home\riddler\cppsim\docs\architecture.md'
---

# cppsim - Epic Breakdown

## Overview

This document provides the complete epic and story breakdown for cppsim, decomposing the requirements from the PRD, UX Design if it exists, and Architecture requirements into implementable stories.

## Requirements Inventory

### Functional Requirements

**Connection & Session Management:**
- **FR1**: Server can accept WebSocket connections from poker clients
- **FR2**: Server can validate client handshake and assign unique session IDs
- **FR3**: Server can detect when a table has 2 connected players and start the game
- **FR4**: Server can detect client disconnections and timeouts during gameplay
- **FR5**: Server can allow disconnected clients to reconnect within a grace period without losing their seat
- **FR6**: Server can remove timed-out clients from the table and award pots accordingly
- **FR7**: Server can inform waiting clients when the table is full (more than 2 connection attempts)

**Poker Game Management:**
- **FR8**: Server can initialize a heads-up NLHE poker game with 2 players
- **FR9**: Server can assign dealer button and blind positions (small blind, big blind)
- **FR10**: Server can shuffle and deal hole cards to each player
- **FR11**: Server can manage betting rounds (preflop, flop, turn, river)
- **FR12**: Server can deal community cards at appropriate betting rounds
- **FR13**: Server can determine hand winners using standard NLHE hand rankings
- **FR14**: Server can calculate and award pots (including side pots for all-in scenarios)
- **FR15**: Server can transition between game phases following poker rules
- **FR16**: Server can start a new hand automatically after the previous hand completes

**Player Actions & Betting:**
- **FR17**: Players can post blinds (small blind, big blind) when required
- **FR18**: Players can fold their hand
- **FR19**: Players can call the current bet
- **FR20**: Players can raise the bet (subject to min/max raise rules)
- **FR21**: Players can check when no bet is required
- **FR22**: Players can go all-in with their remaining stack
- **FR23**: Server can enforce action ordering (players must act in turn)
- **FR24**: Server can enforce bet sizing rules (minimum raise, maximum bet up to stack)
- **FR25**: Server can enforce 30-second action timeouts and auto-fold on timeout

**Chip & Stack Management:**
- **FR26**: Server can assign 100 BB starting stack to each connecting player
- **FR27**: Players can request chip reload when stack falls below 100 BB
- **FR28**: Server can grant chip reloads up to 100 BB (play money, unlimited)
- **FR29**: Server can track player stack balances in real-time
- **FR30**: Server can deduct bets from player stacks
- **FR31**: Server can add won pots to player stacks

**Network & Communication:**
- **FR32**: Server can send game state updates to clients as JSON messages over WebSocket
- **FR33**: Server can receive player actions from clients as JSON messages over WebSocket
- **FR34**: Server can broadcast hand results to all players
- **FR35**: Server can inform players of valid actions available to them
- **FR36**: Server can send error messages to clients for invalid actions
- **FR37**: Server can support protocol versioning in handshake messages

**Security & Validation:**
- **FR38**: Server can validate all incoming client messages against JSON schema
- **FR39**: Server can reject malformed or invalid client messages
- **FR40**: Server can validate that player actions are legal given current game state
- **FR41**: Server can prevent players from acting out of turn
- **FR42**: Server can prevent players from betting more than their stack
- **FR43**: Server can enforce rate limiting (max 1 action per 100ms per client)
- **FR44**: Server can disconnect clients that send excessive invalid actions (> 10 consecutive)
- **FR45**: Server can maintain authoritative game state (never trust client-reported data)

**Logging & Observability:**
- **FR46**: Server can log all connection events (connect, disconnect, reconnect) to console
- **FR47**: Server can log all game actions (folds, calls, raises) with timestamps to console
- **FR48**: Server can log all state transitions (hand start, betting round changes, hand completion) to console
- **FR49**: Server can log all errors and validation failures to console
- **FR50**: Server can log performance metrics (action processing time) to console
- **FR51**: Bot clients can log their hole cards and game view to console
- **FR52**: Bot clients can log their action decisions and reasoning to console
- **FR53**: Bot clients can log all game state updates they receive to console

**Bot Client Behavior:**
- **FR54**: Bot clients can connect to the server via WebSocket
- **FR55**: Bot clients can make random poker decisions (fold, call, raise)
- **FR56**: Bot clients can simulate variable response times (1-3 seconds) before acting
- **FR57**: Bot clients can automatically request chip reloads when busted
- **FR58**: Bot clients can handle disconnections and attempt reconnection
- **FR59**: Bot clients can parse and respond to server game state updates

**Testing & Validation:**
- **FR60**: System can run comprehensive edge case tests (all-in scenarios, disconnections during betting, timeouts, etc.)
- **FR61**: System can run extended stress tests (10,000+ hands without crashes)
- **FR62**: System can run security validation tests (invalid messages, out-of-turn actions, malicious clients)
- **FR63**: System can verify log traceability (any hand can be reconstructed from logs)
- **FR64**: System can measure and validate performance targets (< 50ms action processing, < 100ms state updates)

### NonFunctional Requirements

**Performance:**
- **NFR-P1**: Server action processing must complete within 50ms for any valid poker action
- **NFR-P2**: Server state update broadcasts must complete within 100ms total (serialization + network transmission)
- **NFR-P3**: Timeout detection must have < 1 second granularity for 30-second action timeouts
- **NFR-P4**: Reconnection grace period must support 30 seconds without performance degradation
- **NFR-P5**: Bot client simulated response times must range from 1-3 seconds to simulate human-like play
- **NFR-P6**: JSON serialization/deserialization must not introduce measurable latency (< 5ms per operation)

**Reliability:**
- **NFR-R1**: Server must operate for extended periods (72+ hours) without crashes
- **NFR-R2**: Server must handle 10,000+ consecutive poker hands without failures or memory leaks
- **NFR-R3**: Server must gracefully handle client disconnections without corrupting game state
- **NFR-R4**: Server must maintain game state consistency across all error conditions
- **NFR-R5**: Bot clients must automatically reconnect after network failures without manual intervention
- **NFR-R6**: System must achieve zero rule violations across thousands of hands
- **NFR-R7**: All state transitions must be deterministic and reproducible

**Security:**
- **NFR-S1**: Server must validate 100% of incoming client messages against JSON schema
- **NFR-S2**: Server must reject all malformed or invalid client messages without crashing
- **NFR-S3**: Server must prevent any client action that violates poker rules or game state
- **NFR-S4**: Server must maintain authoritative game state (zero trust of client-reported data)
- **NFR-S5**: Server must enforce rate limiting (max 1 action per 100ms per client)
- **NFR-S6**: Server must disconnect clients exhibiting malicious behavior (> 10 consecutive invalid actions)
- **NFR-S7**: Server must achieve zero exploitable vulnerabilities in security validation testing
- **NFR-S8**: Server must prevent out-of-turn actions, illegal bet sizes, and stack manipulation attempts

**Testability & Observability:**
- **NFR-T1**: All game actions must be logged with sufficient detail to reconstruct any hand from logs alone
- **NFR-T2**: Server logs must capture all connection events, state transitions, errors, and performance metrics
- **NFR-T3**: Bot client logs must provide dual-perspective view (what client sees vs. what server broadcasts)
- **NFR-T4**: System must support comprehensive edge case testing (all-in scenarios, disconnections, timeouts)
- **NFR-T5**: System must support extended stress testing (10,000+ hands) with full observability
- **NFR-T6**: System must provide log traceability to verify correctness of any random hand
- **NFR-T7**: Performance metrics (action processing time, state update latency) must be logged for validation

**Maintainability:**
- **NFR-M1**: Codebase must use clear separation between poker game logic, network protocol, and client simulation
- **NFR-M2**: Single-threaded design must be maintained to ensure deterministic execution and easier debugging
- **NFR-M3**: Architecture must support evolution to multi-table (process-per-table or thread-per-table) without core rewrites
- **NFR-M4**: Protocol design must support versioning for future compatibility
- **NFR-M5**: Code must achieve comprehensive unit test coverage for all poker game logic
- **NFR-M6**: Build system (CMake) must support cross-platform builds without platform-specific hacks

### Additional Requirements

**Architecture Requirements:**

- **Monorepo Structure**: Single CMake project containing server, client, and shared common library for protocol definitions, poker rules, and hand evaluation logic
- **Technology Stack**: C++17, Boost.Beast (WebSocket), Boost.Asio (async I/O), nlohmann/json (JSON serialization), CMake 3.15+ (build system), Google Test (testing framework)
- **Component Architecture**: Component-based design with dependency injection for testability and modularity
- **Functional State Machine**: Game logic implemented using std::variant + std::visit for compile-time state safety
- **Gatekeeper Pattern**: Dedicated ActionValidator component validates all actions before they reach the GameEngine
- **Centralized Event Bus**: All components emit events to event_bus; loggers subscribe for dual-perspective logging without coupling
- **Code-First Protocol**: C++ structs with nlohmann/json macros for type-safe JSON serialization (no external code generation)
- **Naming Conventions**: snake_case for all types, functions, variables; trailing underscore for member variables; #pragma once for header guards
- **Error Handling**: Explicit error handling using std::optional or result<T,E> types; exceptions reserved only for truly exceptional situations
- **Memory Management**: std::unique_ptr for owned resources, raw pointers/references for non-owning access, std::shared_ptr only when genuinely shared
- **Include Ordering**: Own header first, then C system headers, C++ standard library, third-party libraries, project headers
- **Event-Driven Logging**: Components emit events (player_bet_event, hand_complete_event); never call loggers directly
- **Testing Strategy**: Google Test for unit tests, separate integration tests, stress test harness for 10,000+ hands
- **Build System**: CMake FetchContent for dependency management (Boost, nlohmann/json, Google Test); separate targets for server, client, common library, tests

**Starter Template**: Not specified in Architecture - greenfield C++ project with manual setup

### FR Coverage Map

**Connection & Session Management:**
- FR1 (Server accepts WebSocket connections): Epic 2
- FR2 (Validate handshake, assign session IDs): Epic 2
- FR3 (Detect 2 players, start game): Epic 5
- FR4 (Detect disconnections/timeouts): Epic 2
- FR5 (Reconnect within grace period): Epic 2
- FR6 (Remove timed-out clients, award pots): Epic 6
- FR7 (Inform waiting clients when table full): Epic 2

**Poker Game Management:**
- FR8 (Initialize heads-up NLHE game): Epic 3
- FR9 (Assign dealer button, blinds): Epic 3
- FR10 (Shuffle, deal hole cards): Epic 3
- FR11 (Manage betting rounds): Epic 3
- FR12 (Deal community cards): Epic 3
- FR13 (Determine hand winners): Epic 3
- FR14 (Calculate/award pots, side pots): Epic 3
- FR15 (Transition between game phases): Epic 3
- FR16 (Start new hand automatically): Epic 3

**Player Actions & Betting:**
- FR17 (Post blinds): Epic 3
- FR18 (Fold): Epic 3
- FR19 (Call): Epic 3
- FR20 (Raise): Epic 3
- FR21 (Check): Epic 3
- FR22 (All-in): Epic 3
- FR23 (Enforce action ordering): Epic 3
- FR24 (Enforce bet sizing rules): Epic 3
- FR25 (30-second timeout, auto-fold): Epic 6

**Chip & Stack Management:**
- FR26 (Assign 100 BB starting stack): Epic 5
- FR27 (Request chip reload): Epic 5
- FR28 (Grant reloads to 100 BB): Epic 5
- FR29 (Track stack balances real-time): Epic 5
- FR30 (Deduct bets from stacks): Epic 5
- FR31 (Add won pots to stacks): Epic 5

**Network & Communication:**
- FR32 (Send game state updates as JSON): Epic 2
- FR33 (Receive player actions as JSON): Epic 2
- FR34 (Broadcast hand results): Epic 2
- FR35 (Inform valid actions): Epic 2
- FR36 (Send error messages): Epic 2
- FR37 (Protocol versioning in handshake): Epic 2

**Security & Validation:**
- FR38 (Validate messages against JSON schema): Epic 4
- FR39 (Reject malformed/invalid messages): Epic 4
- FR40 (Validate actions are legal): Epic 4
- FR41 (Prevent out-of-turn actions): Epic 4
- FR42 (Prevent betting more than stack): Epic 4
- FR43 (Rate limiting 1 action/100ms): Epic 4
- FR44 (Disconnect after 10 invalid actions): Epic 4
- FR45 (Authoritative game state): Epic 4

**Logging & Observability:**
- FR46 (Log connection events): Epic 7
- FR47 (Log game actions with timestamps): Epic 7
- FR48 (Log state transitions): Epic 7
- FR49 (Log errors/validation failures): Epic 7
- FR50 (Log performance metrics): Epic 7
- FR51 (Client log hole cards/game view): Epic 7
- FR52 (Client log decisions/reasoning): Epic 7
- FR53 (Client log state updates): Epic 7

**Bot Client Behavior:**
- FR54 (Connect to server via WebSocket): Epic 2
- FR55 (Make random poker decisions): Epic 8
- FR56 (Simulate variable response times 1-3s): Epic 8
- FR57 (Auto-request reloads when busted): Epic 8
- FR58 (Handle disconnections, reconnect): Epic 2
- FR59 (Parse/respond to state updates): Epic 8

**Testing & Validation:**
- FR60 (Edge case tests): Epic 9
- FR61 (Stress tests 10,000+ hands): Epic 9
- FR62 (Security validation tests): Epic 9
- FR63 (Log traceability verification): Epic 9
- FR64 (Performance target validation): Epic 9

**Non-Functional Requirements:**
- NFR-P1 to NFR-P6 (Performance): Epic 9
- NFR-R1 to NFR-R7 (Reliability): Epic 9
- NFR-S1 to NFR-S8 (Security): Epic 4
- NFR-T1 to NFR-T7 (Testability/Observability): Epic 7, Epic 9
- NFR-M1 to NFR-M6 (Maintainability): Epic 1, Epic 9

## Epic List

### Epic 1: Project Foundation & Build Infrastructure

**Goal:** Developers can build, test, and develop the poker server with a reproducible, cross-platform build system and coding standards in place.

**User Outcome:** A working CMake-based monorepo with proper project structure, dependency management (Boost.Beast, Boost.Asio, nlohmann/json, Google Test), coding standards enforcement (clang-format, compiler warnings), and the ability to build server, client, and common library targets successfully.

**FRs Covered:** Architecture requirements (CMake setup, monorepo structure, FetchContent dependency management, project directory structure, naming conventions, header patterns, build tooling)

**Implementation Notes:**
- Create complete directory structure as specified in Architecture
- Set up CMakeLists.txt for root, server, client, common, and tests
- Configure CMake FetchContent for all dependencies
- Establish coding standards (.clang-format, compiler warning flags)
- Create placeholder main.cpp files for server and client
- Verify cross-platform builds work

---

### Epic 2: Core Protocol & Network Communication

**Goal:** Server and clients can establish WebSocket connections, exchange JSON messages, and handle connection lifecycle (connect, disconnect, reconnect).

**User Outcome:** A working WebSocket server that accepts client connections, performs handshakes with protocol versioning, exchanges JSON messages bidirectionally, detects disconnections, allows reconnections within a grace period, and informs clients when the table is full.

**FRs Covered:** FR1, FR2, FR4, FR5, FR7, FR32, FR33, FR34, FR35, FR36, FR37, FR54, FR58

**Implementation Notes:**
- Implement Boost.Beast WebSocket server with Boost.Asio event loop
- Define protocol.hpp with JSON message schemas using nlohmann/json macros
- Implement connection_manager for session tracking
- Implement WebSocket client for bots
- Support handshake with protocol version negotiation
- Handle disconnection/reconnection with 30-second grace period
- Implement error message sending for invalid actions

---

### Epic 3: Poker Game Engine & Rules

**Goal:** Server can run complete poker hands following NLHE rules, including hand evaluation, pot calculation, and side pots, with deterministic game state management.

**User Outcome:** A fully functional poker game engine that manages complete hands of heads-up No-Limit Hold'em, including dealer button assignment, blind posting, hole card dealing, all betting rounds (preflop, flop, turn, river), community card dealing, hand evaluation using standard rankings, pot calculation with side pot support, and automatic transition to the next hand.

**FRs Covered:** FR8, FR9, FR10, FR11, FR12, FR13, FR14, FR15, FR16, FR17, FR18, FR19, FR20, FR21, FR22, FR23, FR24

**Implementation Notes:**
- Implement game_engine with std::variant functional state machine
- Create poker_rules library with NLHE game logic
- Implement hand_evaluator for standard hand rankings
- Support all player actions: fold, call, raise, check, all-in
- Enforce action ordering (in-turn validation)
- Enforce bet sizing rules (min raise, max bet to stack)
- Calculate main pot and side pots correctly
- Transition between game phases deterministically

---

### Epic 4: Security, Validation & Rate Limiting

**Goal:** Server rejects invalid actions, malformed messages, and malicious clients, maintaining authoritative game state and preventing all exploit attempts.

**User Outcome:** An authoritative server that validates 100% of client messages against JSON schemas, rejects malformed messages without crashing, prevents illegal actions (out-of-turn, invalid bet sizes, stack manipulation), enforces rate limiting (max 1 action per 100ms), disconnects malicious clients (>10 consecutive invalid actions), and maintains server-side game state as the single source of truth.

**FRs Covered:** FR38, FR39, FR40, FR41, FR42, FR43, FR44, FR45, NFR-S1, NFR-S2, NFR-S3, NFR-S4, NFR-S5, NFR-S6, NFR-S7, NFR-S8

**Implementation Notes:**
- Implement action_validator using Gatekeeper Pattern
- Validate all JSON messages against strict schemas
- Validate action legality given game state
- Enforce in-turn action validation
- Enforce bet sizing constraints (cannot exceed stack)
- Implement rate_limiter (1 action per 100ms per client)
- Track invalid action count, disconnect after 10 consecutive
- Ensure server never trusts client-reported data

---

### Epic 5: Stack Management & Play Money Economy

**Goal:** Players receive starting stacks, can reload when busted, and server tracks all chip movements with perfect accounting.

**User Outcome:** A complete play money economy where players receive 100 BB starting stacks on connection, can request unlimited reloads up to 100 BB when below threshold, and the server tracks all stack balances in real-time with perfect accounting (deducting bets, adding won pots). Game starts automatically when 2 players are seated.

**FRs Covered:** FR26, FR27, FR28, FR29, FR30, FR31, FR3

**Implementation Notes:**
- Implement player_manager for stack tracking
- Assign 100 BB on connection
- Handle reload requests (top up to 100 BB)
- Deduct bets from player stacks
- Add won pots to player stacks
- Ensure real-time balance accuracy
- Trigger game start when 2 players seated

---

### Epic 6: Timeout & Disconnection Handling

**Goal:** Server gracefully handles player timeouts and disconnections without corrupting game state, auto-folding when needed and preserving stacks correctly.

**User Outcome:** Robust timeout and disconnection handling where the server enforces 30-second action timeouts with <1 second detection granularity, auto-folds on timeout while preserving player stack, removes timed-out players and awards pots correctly, supports 30-second reconnection grace period without performance degradation, and never corrupts game state regardless of network failures.

**FRs Covered:** FR25, FR6, NFR-R3, NFR-R4, NFR-P3, NFR-P4

**Implementation Notes:**
- Implement Boost.Asio timers for 30-second action timeouts
- Auto-fold on timeout, preserve player stack
- Award pot correctly when player times out
- Support reconnection grace period (30 seconds)
- Ensure game state consistency during disconnections
- Handle edge cases (timeout during all-in, disconnect during pot calculation)

---

### Epic 7: Dual-Perspective Logging & Observability

**Goal:** All game actions, state transitions, and errors are logged from both server and client perspectives, enabling complete hand reconstruction and debugging.

**User Outcome:** Comprehensive dual-perspective logging system where the server console logs all connection events, game actions with timestamps, state transitions, errors, and performance metrics, while client consoles log their hole cards, game view, action decisions with reasoning, and state updates. Any hand can be completely reconstructed from logs alone.

**FRs Covered:** FR46, FR47, FR48, FR49, FR50, FR51, FR52, FR53, NFR-T1, NFR-T2, NFR-T3, NFR-T6, NFR-T7

**Implementation Notes:**
- Implement event_bus for centralized event publishing
- Implement server_logger subscribing to all events
- Implement client_logger filtering by player visibility
- Log all connection events (connect, disconnect, reconnect)
- Log all game actions with timestamps
- Log all state transitions (hand start, phase changes, completion)
- Log all errors and validation failures
- Log performance metrics (action processing time, state update latency)
- Ensure logs provide complete hand traceability

---

### Epic 8: Bot Client with Strategy & Simulation

**Goal:** Bot clients can play poker with random strategies, human-like timing variability, and automatic reload/reconnect behavior.

**User Outcome:** Fully autonomous bot clients that connect to the server, make random poker decisions (fold, call, raise), simulate human-like response times (1-3 second variable delays), automatically request chip reloads when busted, handle disconnections and attempt reconnection, parse all server state updates, and log their perspective to console.

**FRs Covered:** FR55, FR56, FR57, FR59, NFR-P5, NFR-R5

**Implementation Notes:**
- Implement strategy_engine with random decision making
- Simulate variable response times (1-3 seconds random delay)
- Auto-request reload when stack drops to 0
- Parse JSON state updates from server
- Handle disconnection and auto-reconnect
- Log bot decisions and reasoning to console

---

### Epic 9: Comprehensive Testing & Validation Framework

**Goal:** System has comprehensive test coverage including unit tests, edge case tests, stress tests (10,000+ hands), and security validation tests.

**User Outcome:** A complete validation framework that proves the server is bulletproof through comprehensive unit tests for all poker logic, edge case tests (all-in scenarios, disconnections during betting, timeouts, invalid actions), stress tests running 10,000+ hands without crashes or memory leaks, security validation tests (malicious messages, out-of-turn actions, exploit attempts), performance validation (<50ms action processing, <100ms state updates), and log traceability verification.

**FRs Covered:** FR60, FR61, FR62, FR63, FR64, NFR-T4, NFR-T5, NFR-M5, NFR-R1, NFR-R2, NFR-R6, NFR-R7, NFR-P1, NFR-P2, NFR-P6

**Implementation Notes:**
- Create comprehensive Google Test unit tests for poker_rules, hand_evaluator, game_engine, action_validator
- Create integration tests for server-client interactions
- Create stress test harness for 10,000+ hand runs
- Create security validation tests (malformed messages, invalid actions, malicious clients)
- Verify zero crashes, zero rule violations, zero exploits
- Validate performance targets (action processing <50ms, state updates <100ms)
- Verify log traceability (reconstruct any hand from logs)
- Validate 72+ hour uptime without crashes or memory leaks

<!-- Epic Sections with Stories -->

## Epic 1: Project Foundation & Build Infrastructure

**Goal:** Developers can build, test, and develop the poker server with a reproducible, cross-platform build system and coding standards in place.

### Story 1.1: CMake Project Structure & Dependencies

As a **developer**,
I want **a working CMake build system with all dependencies managed via FetchContent**,
So that **I can build the project on any platform without manual dependency installation**.

**Acceptance Criteria:**

**Given** a fresh clone of the repository
**When** I run `cmake -B build -S .`
**Then** CMake configures successfully and fetches Boost, nlohmann/json, and Google Test
**And** all dependencies are pinned to specific versions
**And** the build system creates separate targets for server, client, common library, and tests

### Story 1.2: Project Directory Structure

As a **developer**,
I want **the complete directory structure following the architecture specification**,
So that **I have organized locations for all code components**.

**Acceptance Criteria:**

**Given** the repository root
**When** I inspect the directory structure
**Then** I see `src/server/`, `src/client/`, `src/common/`, `tests/unit/`, `tests/integration/`, `tests/stress/`, `docs/`, and `cmake/` directories
**And** each directory has appropriate CMakeLists.txt files
**And** placeholder main.cpp files exist for server and client

### Story 1.3: Coding Standards & Style Enforcement

As a **developer**,
I want **clang-format configuration and strict compiler warnings**,
So that **all code follows consistent style and quality standards**.

**Acceptance Criteria:**

**Given** the project is configured
**When** I build with CMake
**Then** compiler warnings are set to pedantic levels (-Wall, -Wextra, -Wpedantic)
**And** a .clang-format file exists enforcing snake_case and other architecture patterns
**And** warnings are treated as errors
**And** the build succeeds with placeholder code

---

## Epic 2: Core Protocol & Network Communication

**Goal:** Server and clients can establish WebSocket connections, exchange JSON messages, and handle connection lifecycle (connect, disconnect, reconnect).

### Story 2.1: Protocol Message Definitions

As a **developer**,
I want **type-safe JSON message schemas defined as C++ structs**,
So that **both server and client can serialize/deserialize messages consistently**.

**Acceptance Criteria:**

**Given** the common library
**When** I define protocol.hpp with nlohmann/json macros
**Then** all message types are defined (HANDSHAKE, ACTION, STATE_UPDATE, ERROR, RELOAD_REQUEST, DISCONNECT)
**And** each message includes protocol version field
**And** unit tests verify JSON serialization/deserialization works correctly

### Story 2.2: WebSocket Server with Connection Management

As a **developer**,
I want **a WebSocket server that accepts client connections**,
So that **clients can connect and establish sessions**.

**Acceptance Criteria:**

**Given** the server is running
**When** a client connects via WebSocket
**Then** the server accepts the connection using Boost.Beast
**And** assigns a unique session ID
**And** tracks the connection in connection_manager
**And** logs the connection event to console

### Story 2.3: Handshake & Protocol Versioning

As a **client**,
I want **to perform a handshake with protocol version negotiation**,
So that **the server validates my client version**.

**Acceptance Criteria:**

**Given** a client connects to the server
**When** the client sends a HANDSHAKE message with protocol version
**Then** the server validates the protocol version
**And** responds with session assignment if version is compatible
**And** rejects connection with error message if version is incompatible
**And** the client receives confirmation and session ID

### Story 2.4: Bidirectional JSON Messaging

As a **server and client**,
I want **to send and receive JSON messages over WebSocket**,
So that **we can communicate game state and actions**.

**Acceptance Criteria:**

**Given** an established WebSocket connection
**When** the server sends a STATE_UPDATE message
**Then** the client receives and deserializes it correctly
**And** when the client sends an ACTION message
**Then** the server receives and deserializes it correctly
**And** both parties log sent/received messages

### Story 2.5: Disconnection Detection

As a **server**,
I want **to detect when clients disconnect**,
So that **I can clean up resources and handle game state appropriately**.

**Acceptance Criteria:**

**Given** a connected client
**When** the client disconnects (close connection or network failure)
**Then** the server detects the disconnection event
**And** logs the disconnection with session ID
**And** marks the session as disconnected in connection_manager
**And** does not crash or leak resources

### Story 2.6: Reconnection with Grace Period

As a **client**,
I want **to reconnect within 30 seconds without losing my seat**,
So that **temporary network issues don't kick me from the game**.

**Acceptance Criteria:**

**Given** a client was previously connected with session ID
**When** the client reconnects within 30 seconds with the same session ID
**Then** the server restores the session
**And** the client receives updated game state
**And** logs show successful reconnection
**And** if reconnection attempt is after 30 seconds, server rejects with error

### Story 2.7: Table Full Notification

As a **server**,
I want **to inform waiting clients when the table is full**,
So that **they know they cannot join**.

**Acceptance Criteria:**

**Given** 2 players are already connected (table full)
**When** a third client tries to connect
**Then** the server sends an error message "Table is full"
**And** rejects the connection
**And** logs the rejected connection attempt

### Story 2.8: WebSocket Bot Client

As a **bot developer**,
I want **a WebSocket client that can connect to the server**,
So that **bots can participate in poker games**.

**Acceptance Criteria:**

**Given** the server is running
**When** I start the bot client
**Then** the client connects via WebSocket using Boost.Beast
**And** sends HANDSHAKE message
**And** receives session ID
**And** can send and receive JSON messages
**And** handles reconnection on disconnect
**And** logs all messages to client console

### Story 2.9: Review Undocumented WebSocket Implementation

As a **developer**,
I want **to review and test the undocumented WebSocket implementation**,
So that **I can ensure it meets quality standards and doesn't introduce technical debt**.

**Acceptance Criteria:**

**Given** the existing WebSocket implementation in src/server/websocket_session.cpp
**When** I review the code
**Then** I verify it follows project architecture patterns
**And** I ensure all edge cases are handled
**And** I add missing documentation and comments
**And** I verify unit tests cover the implementation
**And** I refactor any non-compliant code

---

## Epic 3: Poker Game Engine & Rules

**Goal:** Server can run complete poker hands following NLHE rules, including hand evaluation, pot calculation, and side pots, with deterministic game state management.

### Story 3.1: Game State Machine Foundation

As a **developer**,
I want **a functional state machine using std::variant for game phases**,
So that **invalid state transitions are impossible at compile-time**.

**Acceptance Criteria:**

**Given** the game_engine component
**When** I implement the state machine
**Then** states include: WaitingForPlayers, PreFlop, Flop, Turn, River, Showdown, HandComplete
**And** each state is a distinct struct in std::variant
**And** std::visit is used for state transitions
**And** unit tests verify only valid transitions are possible

### Story 3.2: Hand Initialization & Dealer Button

As a **poker server**,
I want **to initialize a new hand with dealer button assignment**,
So that **blind positions are determined correctly**.

**Acceptance Criteria:**

**Given** 2 players are seated
**When** a new hand starts
**Then** dealer button is assigned (alternates each hand)
**And** small blind and big blind positions are determined based on dealer button
**And** hand number increments
**And** the server logs hand initialization with button position

### Story 3.3: Blind Posting

As a **poker server**,
I want **players to post blinds automatically**,
So that **the pot is seeded correctly**.

**Acceptance Criteria:**

**Given** a new hand has started
**When** blind posting phase begins
**Then** small blind player posts 0.5 BB
**And** big blind player posts 1 BB
**And** pot total equals 1.5 BB
**And** player stacks are deducted correctly
**And** the server logs blind posts

### Story 3.4: Deck Shuffling & Hole Card Dealing

As a **poker server**,
I want **to shuffle a deck and deal hole cards to both players**,
So that **each hand starts with random, private cards**.

**Acceptance Criteria:**

**Given** blinds have been posted
**When** the dealing phase begins
**Then** a 52-card deck is created and shuffled using secure randomization
**And** each player receives 2 hole cards
**And** hole cards are stored privately per player
**And** hole cards are sent to respective clients only
**And** the server logs "Hole cards dealt" without revealing them

### Story 3.5: Preflop Betting Round

As a **poker server**,
I want **to manage the preflop betting round**,
So that **players can make betting decisions before the flop**.

**Acceptance Criteria:**

**Given** hole cards have been dealt
**When** preflop betting begins
**Then** action starts with small blind player (button)
**And** players can fold, call, or raise
**And** betting continues until both players have acted and bets are equal
**And** the server requests action from the correct player
**And** the server tracks current bet amount and pot size
**And** transitions to flop when betting round completes

### Story 3.6: Community Card Dealing (Flop, Turn, River)

As a **poker server**,
I want **to deal community cards at appropriate betting rounds**,
So that **the hand progresses through flop, turn, and river**.

**Acceptance Criteria:**

**Given** a betting round has completed
**When** transitioning to flop
**Then** 3 community cards are dealt and broadcast to both players
**And** when transitioning to turn, 1 additional card is dealt
**And** when transitioning to river, 1 final card is dealt
**And** community cards are visible to both players
**And** each dealing is logged to console

### Story 3.7: Betting Rounds (Flop, Turn, River)

As a **poker server**,
I want **to manage betting rounds for flop, turn, and river**,
So that **players can bet on each street**.

**Acceptance Criteria:**

**Given** community cards have been dealt
**When** a betting round begins
**Then** action starts with big blind player
**And** players can check, bet, call, raise, or fold
**And** betting continues until both players have acted and bets are equal
**And** the server enforces turn order
**And** transitions to next phase when round completes
**And** if a player folds, hand ends immediately

### Story 3.8: Player Actions (Fold, Call, Check)

As a **player**,
I want **to fold, call, or check during my turn**,
So that **I can make basic poker decisions**.

**Acceptance Criteria:**

**Given** it is my turn to act
**When** I choose to fold
**Then** my hand is mucked and I lose the pot
**And** when I choose to call, the difference between my bet and current bet is deducted from my stack
**And** when I choose to check (if no bet), no chips are wagered
**And** the server validates each action is legal
**And** logs the action with player ID and amount

### Story 3.9: Player Actions (Raise)

As a **player**,
I want **to raise the current bet**,
So that **I can increase pressure on my opponent**.

**Acceptance Criteria:**

**Given** it is my turn to act and there's a bet to me
**When** I raise
**Then** the server validates the raise amount meets minimum raise rules (at least double the current bet or previous raise)
**And** validates I have sufficient stack
**And** deducts the raise amount from my stack
**And** updates the current bet amount
**And** logs the raise action

### Story 3.10: All-In Handling

As a **player**,
I want **to go all-in with my remaining stack**,
So that **I can commit all my chips even if I'm short-stacked**.

**Acceptance Criteria:**

**Given** it is my turn to act
**When** I go all-in
**Then** my entire remaining stack is committed to the pot
**And** the server marks me as all-in
**And** if opponent has more chips, excess chips are tracked for side pot
**And** all remaining betting rounds are automatic (I cannot act further)
**And** logs show all-in action with amount

### Story 3.11: Hand Evaluation & Winner Determination

As a **poker server**,
I want **to evaluate hands using standard NLHE rankings**,
So that **I can determine the winner at showdown**.

**Acceptance Criteria:**

**Given** the river betting round is complete (or all players are all-in)
**When** showdown occurs
**Then** the hand_evaluator ranks each player's best 5-card hand
**And** determines the winner using standard rankings (Royal Flush > Straight Flush > Four of a Kind > ... > High Card)
**And** handles ties (split pot)
**And** logs the winning hand and player

### Story 3.12: Pot Calculation & Side Pots

As a **poker server**,
I want **to calculate and award pots including side pots for all-in scenarios**,
So that **chip distribution is always correct**.

**Acceptance Criteria:**

**Given** a hand has completed
**When** calculating pots
**Then** main pot is calculated correctly for matched bets
**And** side pots are created when a player is all-in with less than opponent's bet
**And** each pot is awarded to the correct winner
**And** player stacks are updated with winnings
**And** logs show pot breakdown and awards

### Story 3.13: Automatic Hand Transition

As a **poker server**,
I want **to automatically start a new hand after the previous hand completes**,
So that **the game continues seamlessly**.

**Acceptance Criteria:**

**Given** a hand has completed (pot awarded)
**When** hand cleanup finishes
**Then** dealer button rotates to the other player
**And** a new hand is initialized automatically
**And** blinds are posted for the new hand
**And** the cycle continues
**And** logs show hand transition with new hand number

---

## Epic 4: Security, Validation & Rate Limiting

**Goal:** Server rejects invalid actions, malformed messages, and malicious clients, maintaining authoritative game state and preventing all exploit attempts.

### Story 4.1: JSON Schema Validation

As a **server**,
I want **to validate all incoming messages against strict JSON schemas**,
So that **malformed messages are rejected immediately**.

**Acceptance Criteria:**

**Given** a client sends a JSON message
**When** the server receives it
**Then** the message is validated against the expected schema for its type
**And** validation checks for required fields, correct types, and value ranges
**And** malformed messages are rejected with descriptive error
**And** well-formed messages proceed to next validation layer
**And** the server never crashes on malformed JSON

### Story 4.2: Action Validator (Gatekeeper Pattern)

As a **server**,
I want **a dedicated ActionValidator component that validates action legality**,
So that **invalid actions never reach the game engine**.

**Acceptance Criteria:**

**Given** an ACTION message has passed JSON schema validation
**When** the ActionValidator processes it
**Then** it checks if the action is legal given current game state
**And** validates player is allowed to act (their turn)
**And** validates action type is valid for current phase
**And** validates bet amounts are within legal range
**And** returns validation result (success or specific error)
**And** invalid actions are rejected before reaching game_engine

### Story 4.3: Turn Order Enforcement

As a **server**,
I want **to prevent players from acting out of turn**,
So that **only the current player can submit actions**.

**Acceptance Criteria:**

**Given** it is Player A's turn
**When** Player B sends an action
**Then** the ActionValidator rejects it with "Not your turn" error
**And** when Player A sends an action, it is validated and processed
**And** out-of-turn attempts are logged
**And** the game state remains unchanged after rejection

### Story 4.4: Bet Sizing Validation

As a **server**,
I want **to enforce bet sizing rules**,
So that **players cannot bet more than their stack or make illegal raises**.

**Acceptance Criteria:**

**Given** a player attempts to bet or raise
**When** the ActionValidator checks the amount
**Then** it rejects bets exceeding player's stack
**And** rejects raises below minimum raise amount
**And** validates call amounts match the bet to player
**And** allows all-in as special case (entire stack)
**And** returns specific error for each violation
**And** logs invalid bet attempts

### Story 4.5: Rate Limiting

As a **server**,
I want **to enforce rate limiting of 1 action per 100ms per client**,
So that **clients cannot spam actions to DoS the server**.

**Acceptance Criteria:**

**Given** a client is connected
**When** the client sends actions
**Then** the rate_limiter tracks last action timestamp per client
**And** rejects actions sent within 100ms of previous action
**And** allows actions after 100ms has elapsed
**And** rate limit violations are logged
**And** excessive violations trigger malicious behavior detection

### Story 4.6: Malicious Client Detection & Disconnection

As a **server**,
I want **to disconnect clients sending excessive invalid actions**,
So that **malicious clients cannot disrupt the game**.

**Acceptance Criteria:**

**Given** a client is connected
**When** the client sends invalid actions
**Then** the server tracks consecutive invalid action count
**And** when count exceeds 10 consecutive invalid actions, client is disconnected
**And** counter resets on any valid action
**And** disconnection is logged with reason
**And** the game continues without the malicious client

### Story 4.7: Authoritative Game State

As a **server**,
I want **to maintain all game state server-side and never trust client data**,
So that **clients cannot manipulate game state**.

**Acceptance Criteria:**

**Given** the server is running
**When** managing game state
**Then** all cards, stacks, pots, and game phase are stored server-side only
**And** clients never send state data (only actions)
**And** the server broadcasts state updates to clients
**And** the server ignores any client-reported state data
**And** unit tests verify clients cannot influence state directly

### Story 4.8: Security Validation Testing

As a **developer**,
I want **comprehensive security tests for all validation rules**,
So that **I can prove the server is exploit-resistant**.

**Acceptance Criteria:**

**Given** the security validation test suite
**When** tests run
**Then** tests verify JSON schema validation catches all malformed messages
**And** tests verify ActionValidator rejects all illegal actions
**And** tests verify rate limiting works correctly
**And** tests verify malicious client detection disconnects bad actors
**And** tests verify authoritative state cannot be manipulated
**And** all tests pass with zero exploits found

---

## Epic 5: Stack Management & Play Money Economy

**Goal:** Players receive starting stacks, can reload when busted, and server tracks all chip movements with perfect accounting.

### Story 5.1: Player Manager Component

As a **developer**,
I want **a PlayerManager component that tracks all player stacks**,
So that **chip accounting is centralized and accurate**.

**Acceptance Criteria:**

**Given** the server is running
**When** PlayerManager is initialized
**Then** it maintains a map of session_id to player stack balance
**And** provides methods to get_stack(), deduct_chips(), add_chips()
**And** all stack operations are atomic
**And** unit tests verify stack operations are accurate

### Story 5.2: Starting Stack Assignment

As a **player**,
I want **to receive 100 BB starting stack when I connect**,
So that **I can start playing immediately**.

**Acceptance Criteria:**

**Given** a player successfully completes handshake
**When** the session is created
**Then** PlayerManager assigns 100 BB to the player's stack
**And** the player receives a STATE_UPDATE with their stack amount
**And** the server logs stack assignment
**And** stack is tracked in real-time

### Story 5.3: Game Start on 2 Players

As a **server**,
I want **to automatically start the game when 2 players are seated**,
So that **the poker game begins once the table is full**.

**Acceptance Criteria:**

**Given** 1 player is connected
**When** a second player connects and receives starting stack
**Then** the server detects 2 players are seated
**And** game_engine initializes the first hand
**And** dealer button is assigned
**And** the server broadcasts "Game starting" to both players
**And** hand #1 begins automatically

### Story 5.4: Chip Deduction on Bets

As a **server**,
I want **to deduct chips from player stacks when they bet**,
So that **chip accounting is accurate during gameplay**.

**Acceptance Criteria:**

**Given** a player makes a bet during a hand
**When** the bet is validated and processed
**Then** PlayerManager deducts the bet amount from player's stack
**And** the pot is updated with the bet amount
**And** the player receives STATE_UPDATE with new stack balance
**And** stack never goes negative (validated by ActionValidator)
**And** all chip movements are logged

### Story 5.5: Pot Award on Win

As a **server**,
I want **to add won pots to player stacks**,
So that **winners receive their chips correctly**.

**Acceptance Criteria:**

**Given** a hand has completed and winner is determined
**When** the pot is awarded
**Then** PlayerManager adds the pot amount to winner's stack
**And** the winner receives STATE_UPDATE with new stack balance
**And** pot is reset to 0 for next hand
**And** all awards are logged with amount and recipient
**And** stack updates are accurate to the chip

### Story 5.6: Reload Request Handling

As a **player**,
I want **to request a chip reload when my stack falls below 100 BB**,
So that **I can continue playing after losses**.

**Acceptance Criteria:**

**Given** a player's stack is below 100 BB
**When** the player sends a RELOAD_REQUEST message
**Then** the server validates the request
**And** PlayerManager tops up the player's stack to 100 BB
**And** the player receives STATE_UPDATE with new stack (100 BB)
**And** reload is logged with amount added
**And** reload can be requested unlimited times (play money)

### Story 5.7: Real-Time Stack Tracking

As a **server**,
I want **to track player stack balances in real-time**,
So that **stack information is always current and accurate**.

**Acceptance Criteria:**

**Given** gameplay is in progress
**When** any stack-affecting event occurs (bet, pot award, reload)
**Then** PlayerManager updates the stack immediately
**And** STATE_UPDATE is broadcast to affected player
**And** stack balance is consistent across all server components
**And** logs show real-time stack changes
**And** unit tests verify stack tracking accuracy across 1000+ transactions

---

## Epic 6: Timeout & Disconnection Handling

**Goal:** Server gracefully handles player timeouts and disconnections without corrupting game state, auto-folding when needed and preserving stacks correctly.

### Story 6.1: Action Timeout Timer

As a **server**,
I want **to enforce 30-second action timeouts using Boost.Asio timers**,
So that **slow players don't stall the game indefinitely**.

**Acceptance Criteria:**

**Given** it is a player's turn to act
**When** the server requests action
**Then** a 30-second Boost.Asio timer is started
**And** if player acts before timeout, timer is cancelled
**And** if player doesn't act within 30 seconds, timeout event fires
**And** timeout detection has <1 second granularity
**And** the server logs timeout events with timestamp

### Story 6.2: Auto-Fold on Timeout

As a **server**,
I want **to automatically fold a player's hand on timeout**,
So that **the game continues even when a player doesn't respond**.

**Acceptance Criteria:**

**Given** a player's action timer has expired
**When** the timeout event fires
**Then** the server automatically folds the player's hand
**And** player's stack is preserved (no additional chips wagered)
**And** the pot is awarded to the opponent
**And** the hand completes normally
**And** logs show "Player timed out - auto-folded"

### Story 6.3: Pot Award on Player Timeout

As a **server**,
I want **to correctly award the pot when a player times out**,
So that **the opponent receives chips they're entitled to**.

**Acceptance Criteria:**

**Given** a player has timed out and been auto-folded
**When** the hand completes
**Then** the pot is awarded to the remaining active player
**And** PlayerManager adds pot to winner's stack
**And** both players receive STATE_UPDATE with hand result
**And** logs show pot award with reason "opponent timeout"
**And** chip accounting is perfectly accurate

### Story 6.4: Reconnection Grace Period

As a **server**,
I want **to support 30-second reconnection grace period without performance degradation**,
So that **temporarily disconnected players can resume their session**.

**Acceptance Criteria:**

**Given** a player disconnects during gameplay
**When** the disconnection is detected
**Then** the server starts a 30-second grace period timer
**And** player's seat and stack are preserved
**And** if player reconnects within 30 seconds, session resumes
**And** if player doesn't reconnect, they are removed after grace period
**And** performance metrics show no degradation during grace period

### Story 6.5: Game State Preservation on Disconnect

As a **server**,
I want **to preserve game state integrity when players disconnect**,
So that **disconnections never corrupt the game state**.

**Acceptance Criteria:**

**Given** a player disconnects mid-hand
**When** the disconnection occurs
**Then** the server marks player as disconnected
**And** current game state (pot, stacks, cards, phase) remains intact
**And** if player's turn, timeout logic applies
**And** if player reconnects, they receive current game state
**And** unit tests verify state consistency across disconnect scenarios
**And** no memory leaks or corruption occurs

### Story 6.6: Edge Case Handling (Disconnect During All-In)

As a **server**,
I want **to handle disconnections during all-in scenarios correctly**,
So that **chip distribution is accurate even on disconnect**.

**Acceptance Criteria:**

**Given** a player is all-in and then disconnects
**When** the hand completes
**Then** the server evaluates the hand normally
**And** pot is distributed correctly based on hand outcome
**And** player's stack is updated (even while disconnected)
**And** if player reconnects, they see correct stack balance
**And** logs show complete hand history despite disconnect

### Story 6.7: Timeout During Pot Calculation

As a **server**,
I want **to handle edge cases like timeout during pot calculation**,
So that **all timing edge cases are covered**.

**Acceptance Criteria:**

**Given** various edge case scenarios (timeout during all-in, disconnect during pot award, etc.)
**When** these scenarios occur
**Then** the server completes the current game phase atomically
**And** never leaves game state in inconsistent state
**And** integration tests verify all edge case scenarios
**And** logs provide complete traceability for debugging
**And** zero crashes occur across all tested scenarios

---

## Epic 7: Dual-Perspective Logging & Observability

**Goal:** All game actions, state transitions, and errors are logged from both server and client perspectives, enabling complete hand reconstruction and debugging.

### Story 7.1: Centralized Event Bus

As a **developer**,
I want **a centralized event_bus for publishing and subscribing to all game events**,
So that **logging is decoupled from business logic**.

**Acceptance Criteria:**

**Given** the common library
**When** event_bus is implemented
**Then** it supports publish() method for emitting events
**And** supports subscribe() method for registering listeners
**And** events include types: connection_event, action_event, state_transition_event, error_event, performance_event
**And** all components can publish events without direct logger coupling
**And** unit tests verify event publishing and subscription

### Story 7.2: Server Logger (Connection Events)

As a **server**,
I want **to log all connection events to console**,
So that **I can trace client connections and disconnections**.

**Acceptance Criteria:**

**Given** the server_logger is subscribed to the event_bus
**When** connection events occur (connect, disconnect, reconnect)
**Then** server_logger logs each event with timestamp, session_id, and event type
**And** logs include "Player connected: session_id=X"
**And** logs include "Player disconnected: session_id=X"
**And** logs include "Player reconnected: session_id=X"
**And** all logs are written to server console (stdout)

### Story 7.3: Server Logger (Game Actions)

As a **server**,
I want **to log all game actions with timestamps**,
So that **every player action is traceable**.

**Acceptance Criteria:**

**Given** the server_logger is subscribed to action_events
**When** players perform actions (fold, call, raise, check, all-in)
**Then** server_logger logs each action with timestamp, player_id, action_type, amount
**And** logs include "Player A folds"
**And** logs include "Player B raises to 6 BB"
**And** logs include "Player A calls 3 BB"
**And** logs show chronological action sequence for every hand

### Story 7.4: Server Logger (State Transitions)

As a **server**,
I want **to log all state transitions**,
So that **hand progression is fully documented**.

**Acceptance Criteria:**

**Given** the server_logger is subscribed to state_transition_events
**When** game state changes occur
**Then** server_logger logs hand start with hand number and dealer button
**And** logs betting round changes (PreFlop → Flop → Turn → River)
**And** logs hand completion with winner and pot amount
**And** logs include "Hand #42 started, button: Player A"
**And** logs include "Dealing flop: [Kh, 9d, 3c]"

### Story 7.5: Server Logger (Errors & Performance Metrics)

As a **server**,
I want **to log all errors and performance metrics**,
So that **debugging and performance analysis are possible**.

**Acceptance Criteria:**

**Given** the server_logger is subscribed to error_events and performance_events
**When** errors or performance data are generated
**Then** all validation failures are logged with error details
**And** all malformed messages are logged
**And** action processing time is logged (in milliseconds)
**And** state update latency is logged
**And** logs include "Action processing took 15ms"

### Story 7.6: Client Logger (Game View & Hole Cards)

As a **bot client**,
I want **to log my hole cards and game view to console**,
So that **I can see what my bot perceives**.

**Acceptance Criteria:**

**Given** the client_logger is subscribed to client-side events
**When** I receive hole cards
**Then** client_logger logs "Received hole cards: [As, Kh]"
**And** when I receive STATE_UPDATE, logs pot size, stacks, community cards
**And** logs include "Community cards: [Kh, 9d, 3c]"
**And** logs include "My stack: 97 BB, Opponent stack: 103 BB, Pot: 6 BB"
**And** all logs written to client console (stdout)

### Story 7.7: Client Logger (Decisions & Reasoning)

As a **bot client**,
I want **to log my action decisions and reasoning**,
So that **bot behavior is transparent and debuggable**.

**Acceptance Criteria:**

**Given** the bot strategy_engine makes a decision
**When** the decision is made
**Then** client_logger logs the action chosen and reasoning
**And** logs include "Action: Raise to 6 BB (random strategy)"
**And** logs include "Action: Fold (random strategy)"
**And** logs include simulated thinking time delay
**And** all decisions are timestamped

### Story 7.8: Client Logger (State Updates)

As a **bot client**,
I want **to log all state updates I receive**,
So that **I can verify server communication**.

**Acceptance Criteria:**

**Given** the client receives STATE_UPDATE messages
**When** updates arrive
**Then** client_logger logs each update with message type and content
**And** logs include "Received STATE_UPDATE: Opponent raises to 6 BB"
**And** logs include "Received STATE_UPDATE: Hand complete, winner: Player B"
**And** client logs provide complete view of what client perceives

### Story 7.9: Hand Traceability Verification

As a **developer**,
I want **to verify any hand can be reconstructed from logs alone**,
So that **complete traceability is guaranteed**.

**Acceptance Criteria:**

**Given** server and client logs from a complete hand
**When** I review the logs
**Then** I can reconstruct every action in chronological order
**And** I can verify pot calculations from logged bets
**And** I can verify winner determination from logged hands
**And** dual perspectives (server + client) match for all events
**And** unit tests verify log completeness for sample hands

---

## Epic 8: Bot Client with Strategy & Simulation

**Goal:** Bot clients can play poker with random strategies, human-like timing variability, and automatic reload/reconnect behavior.

### Story 8.1: Strategy Engine with Random Decisions

As a **bot developer**,
I want **a strategy_engine that makes random poker decisions**,
So that **bots can play autonomously with unpredictable behavior**.

**Acceptance Criteria:**

**Given** the bot receives valid actions from server (e.g., "call, raise, fold")
**When** strategy_engine chooses an action
**Then** it randomly selects one of the valid actions
**And** for raises, randomly chooses a raise amount within legal range
**And** decision distribution is approximately uniform across valid actions
**And** unit tests verify randomness and legality of decisions

### Story 8.2: Human-Like Timing Simulation

As a **bot client**,
I want **to simulate variable response times (1-3 seconds) before acting**,
So that **bot behavior resembles human players and creates realistic test conditions**.

**Acceptance Criteria:**

**Given** the bot has decided on an action
**When** sending the action to the server
**Then** the bot waits a random delay between 1-3 seconds before sending
**And** delay uses uniform or normal distribution
**And** actual response time for each action is logged
**And** performance tests verify response times are within 1-3 second range
**And** delays don't violate 30-second server timeout

### Story 8.3: State Update Parsing

As a **bot client**,
I want **to parse and respond to all server state updates**,
So that **I maintain current game state and can make informed decisions**.

**Acceptance Criteria:**

**Given** the bot receives STATE_UPDATE messages from server
**When** parsing the message
**Then** the bot extracts hole cards, community cards, pot size, stacks, valid actions
**And** updates internal game state representation
**And** determines if action is required
**And** logs the parsed state to console
**And** handles all message types correctly (hand start, betting round change, hand complete)

### Story 8.4: Automatic Reload on Bust

As a **bot client**,
I want **to automatically request chip reload when my stack reaches 0**,
So that **I can continue playing indefinitely**.

**Acceptance Criteria:**

**Given** the bot's stack drops to 0 BB (busted)
**When** the bot detects the bust condition
**Then** it immediately sends a RELOAD_REQUEST message
**And** waits for server confirmation
**And** updates local stack to 100 BB upon receipt
**And** logs "Stack depleted. Requesting reload to 100 BB"
**And** resumes playing automatically after reload

### Story 8.5: Automatic Reconnection on Disconnect

As a **bot client**,
I want **to handle disconnections and automatically attempt reconnection**,
So that **network failures don't permanently stop the bot**.

**Acceptance Criteria:**

**Given** the bot detects a disconnection (WebSocket close or error)
**When** the disconnection occurs
**Then** the bot logs the disconnection event
**And** attempts to reconnect after 2-5 seconds
**And** if reconnection succeeds, resumes with previous session_id
**And** if reconnection fails, retries up to 3 times
**And** logs all reconnection attempts
**And** handles network failures gracefully without crashing

### Story 8.6: Multi-Bot Launcher

As a **developer**,
I want **to launch multiple bot clients simultaneously for testing**,
So that **I can easily start games with 2 bots**.

**Acceptance Criteria:**

**Given** the bot client executable
**When** I run the launcher script
**Then** 2 bot clients start and connect to the server
**And** each bot has a unique identifier (Bot_Alpha, Bot_Beta, etc.)
**And** both bots can play a complete game
**And** logs from each bot are clearly identified
**And** script supports command-line arguments (server address, bot count)

---

## Epic 9: Comprehensive Testing & Validation Framework

**Goal:** System has comprehensive test coverage including unit tests, edge case tests, stress tests (10,000+ hands), and security validation tests.

### Story 9.1: Unit Tests for Poker Rules

As a **developer**,
I want **comprehensive Google Test unit tests for poker_rules and hand_evaluator**,
So that **NLHE game logic correctness is proven**.

**Acceptance Criteria:**

**Given** the poker_rules and hand_evaluator components
**When** unit tests run
**Then** all hand rankings are verified (Royal Flush → High Card)
**And** pot calculation logic is tested (including side pots)
**And** betting round logic is tested
**And** edge cases like ties and all-ins are covered
**And** all tests pass with 100% coverage of game logic
**And** tests use Google Test framework

### Story 9.2: Unit Tests for Game Engine

As a **developer**,
I want **unit tests for the game_engine state machine**,
So that **state transitions are verified to be correct**.

**Acceptance Criteria:**

**Given** the game_engine component
**When** unit tests run
**Then** all valid state transitions are tested
**And** invalid state transitions are proven impossible
**And** hand progression (PreFlop → Flop → Turn → River → Showdown) is verified
**And** tests use mocked dependencies (network, player manager)
**And** all tests pass verifying FSM correctness

### Story 9.3: Unit Tests for Action Validator

As a **developer**,
I want **unit tests for ActionValidator (Gatekeeper Pattern)**,
So that **validation rules are proven correct**.

**Acceptance Criteria:**

**Given** the action_validator component
**When** unit tests run
**Then** all validation rules are tested (turn order, bet sizing, legal actions)
**And** valid actions pass validation
**And** invalid actions are correctly rejected with specific errors
**And** edge cases (all-in, minimum raise) are covered
**And** all tests pass with 100% validation coverage

### Story 9.4: Integration Tests (Server-Client Handshake)

As a **developer**,
I want **integration tests for server-client interactions**,
So that **end-to-end communication is verified**.

**Acceptance Criteria:**

**Given** integration test suite
**When** tests run
**Then** server-client handshake is tested end-to-end
**And** JSON message serialization/deserialization is verified
**And** connection/disconnection scenarios are tested
**And** tests verify both client and server perspectives
**And** all integration tests pass

### Story 9.5: Integration Tests (Complete Hand Playthrough)

As a **developer**,
I want **integration tests for complete poker hands**,
So that **full game flow is verified end-to-end**.

**Acceptance Criteria:**

**Given** integration test suite
**When** tests run
**Then** complete hands are played from start to finish
**And** all betting rounds are verified
**And** pot awards are verified
**And** stack updates are verified
**And** tests cover various scenarios (folds, all-ins, showdowns)
**And** all hand flow tests pass

### Story 9.6: Edge Case Test Suite

As a **developer**,
I want **comprehensive edge case tests**,
So that **all NLHE and network edge cases are proven handled**.

**Acceptance Criteria:**

**Given** edge case test suite
**When** tests run
**Then** all-in scenarios (both players all-in, one all-in) are tested
**And** disconnections during all phases are tested
**And** timeouts during betting are tested
**And** invalid action attempts are tested
**And** side pot calculations are tested
**And** all edge cases pass without crashes or corruption

### Story 9.7: Stress Test Harness (10,000+ Hands)

As a **developer**,
I want **a stress test harness that runs 10,000+ hands automatically**,
So that **server reliability is proven under extended load**.

**Acceptance Criteria:**

**Given** the stress test harness
**When** I run the test
**Then** 10,000+ hands are played using bot clients
**And** server runs continuously without crashes
**And** no memory leaks detected (valgrind or similar)
**And** zero rule violations detected
**And** zero game state corruption detected
**And** test completes successfully with summary report

### Story 9.8: Security Validation Test Suite

As a **developer**,
I want **security validation tests for all exploit scenarios**,
So that **the server is proven exploit-resistant**.

**Acceptance Criteria:**

**Given** security validation test suite
**When** tests run
**Then** malformed JSON messages are sent and rejected correctly
**And** out-of-turn actions are sent and rejected
**And** invalid bet amounts are sent and rejected
**And** rapid action spam is sent and rate limited
**And** client with 10+ consecutive invalid actions is disconnected
**And** all security tests pass with zero exploits found

### Story 9.9: Performance Validation Tests

As a **developer**,
I want **performance validation tests**,
So that **performance targets are proven met**.

**Acceptance Criteria:**

**Given** performance test suite
**When** tests run
**Then** action processing latency is measured and verified <50ms
**And** state update broadcast latency is measured and verified <100ms
**And** JSON serialization latency is verified <5ms
**And** timeout detection granularity is verified <1 second
**And** all performance targets are met
**And** performance metrics are logged for analysis

### Story 9.10: Log Traceability Verification

As a **developer**,
I want **to verify any hand can be reconstructed from logs**,
So that **complete traceability is proven**.

**Acceptance Criteria:**

**Given** logs from completed hands
**When** log analysis tool runs
**Then** random hands are selected for verification
**And** complete action sequence is reconstructed from server logs
**And** client perspective is reconstructed from client logs
**And** both perspectives match for all events
**And** pot calculations are verified from log data
**And** traceability verification passes for all sampled hands

### Story 9.11: 72-Hour Uptime Test

As a **developer**,
I want **to run the server for 72+ hours continuously**,
So that **extended uptime reliability is proven**.

**Acceptance Criteria:**

**Given** the complete system is deployed
**When** I run the 72-hour test
**Then** server operates continuously for 72+ hours
**And** bot clients play continuously with auto-reconnect
**And** zero crashes occur
**And** zero memory leaks detected
**And** performance remains consistent throughout
**And** final report shows uptime, hands played, and reliability metrics
