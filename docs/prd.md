---
stepsCompleted: [1, 2, 3, 4, 6, 7, 8, 9, 10, 11]
inputDocuments: []
documentCounts:
  briefs: 0
  research: 0
  brainstorming: 0
  projectDocs: 0
workflowType: 'prd'
lastStep: 11
project_name: 'cppsim'
user_name: 'Riddler'
date: '2025-12-10T23:15:19+08:00'
---

# Product Requirements Document - cppsim

**Author:** Riddler
**Date:** 2025-12-10T23:15:19+08:00

## Executive Summary

**cppsim** is a C++ poker server simulation designed to reliably host heads-up No-Limit Hold'em (NLHE) cash games. This project serves as a foundation for building a robust, scalable multi-table poker server by first proving reliability with a single table hosting two players.

The system consists of:
- A **C++ server** that manages a single heads-up NLHE table, handles client connections, manages game state, processes poker actions, and gracefully handles disconnections and timeouts
- **C++ bot clients** that connect to the server, play with randomized strategies and variable response times (simulating human behavior), automatically request reloads when busted, and log all game information from their perspective
- **Play money economy**: Each player receives 100 BB (big blinds) on connection and can top up to 100 BB when below that threshold

The primary focus is on **server reliability and observability** through comprehensive logging of all server actions and client decisions, enabling validation of game mechanics and network robustness.

### What Makes This Special

- **Observability-First Design**: Dual-perspective logging (server + client consoles) provides complete visibility into game flow, state management, and network interactions
- **Human-Like Simulation**: Bots use variable response times to create realistic testing conditions that expose edge cases and timing-sensitive bugs
- **Reliability-Focused MVP**: Deliberately scoped to prove single-table reliability before scaling to multiple tables
- **Foundation Architecture**: Built as a testbed to validate server patterns and mechanics that will support future multi-table expansion

## Project Classification

**Technical Type:** API/Backend Server (network server with game logic)
**Domain:** Scientific/Simulation (testing and validation platform)
**Complexity:** Medium
**Project Context:** Greenfield - new project

**Technical Characteristics:**
- Network programming (client-server architecture)
- Real-time game state management
- Concurrent connection handling
- Event-driven architecture for poker actions
- No persistence layer (in-memory state only)
- Comprehensive logging for validation and debugging

## Success Criteria

### User Success (Validation Perspective)

The primary "user" of this system is you, the developer/architect, validating the server's reliability and safety. Success means:

- **Reliability Confidence**: The server demonstrates stability under all tested conditions, giving confidence to build multi-table support
- **Edge Case Coverage**: All networking scenarios (disconnections, timeouts, reconnections) are handled gracefully without crashes or corruption
- **Game Integrity Assurance**: Every poker hand follows NLHE rules perfectly, with no invalid game states possible
- **Security Validation**: Malicious or buggy clients cannot exploit, crash, or corrupt the server
- **Complete Observability**: Dual-perspective logging (server + client) provides full traceability to debug and validate any scenario

### Business Success

Since this is a foundational project, business success is measured by **confidence to scale**:

- **Proven Foundation**: Extensive testing validates the architecture can support future expansion (multi-table, deposits/withdrawals, mental poker)
- **Quality Over Speed**: No timeline pressure - the MVP is battle-tested until all edge cases are proven handled
- **Maintainable Codebase**: Clean architecture that supports the roadmap from traditional server → mental poker protocol
- **Documentation Through Logs**: Comprehensive logging serves as validation documentation and debugging tool

### Technical Success

- **Zero Exploits**: Security validation shows no way for clients to manipulate, crash, or corrupt the server
- **100% Rule Compliance**: Every poker hand follows NLHE rules with perfect game state management
- **Graceful Degradation**: Network issues (disconnections, timeouts) never crash the server or corrupt game state
- **Complete Observability**: Logs capture every decision, state change, and action from both server and client perspectives
- **Input Validation**: All client messages are validated; invalid actions are rejected safely

### Measurable Outcomes

- **Comprehensive Test Coverage**: All edge cases explicitly tested (timeouts during betting, disconnections during all-in, invalid actions, malicious messages)
- **Extended Stress Testing**: Server runs extensive simulations (thousands of hands) without crashes, rule violations, or memory leaks
- **Log Traceability**: Any hand can be traced through both server and client logs to verify correctness
- **Security Test Pass**: Invalid client messages (wrong bet amounts, out-of-turn actions, malformed requests) are rejected correctly
- **Network Resilience Validation**: All disconnection/timeout scenarios handled without breaking game state

## Product Scope

### MVP - Minimum Viable Product

**Goal**: Prove single-table reliability before scaling

**Core Features**:
- **Single Heads-Up Table**: One table supporting exactly 2 players (heads-up NLHE)
- **Complete Poker Rules**: Full NLHE game logic (blinds, betting rounds, hand evaluation, pot management, side pots)
- **Play Money Economy**: 
  - 100 BB starting stack on connection
  - Auto top-up to 100 BB when below threshold
  - No persistence or database
- **Network Server**: 
  - TCP/IP client-server architecture
  - Connection management
  - Graceful disconnect handling
  - Timeout detection and enforcement
- **Security & Validation**:
  - All client messages validated
  - Invalid actions rejected safely
  - Protection against malicious clients
- **Bot Clients**:
  - Random strategy implementation
  - Variable response times (human-like)
  - Auto-reload on bust-out
  - Client-side logging
- **Comprehensive Logging**:
  - Server console: All actions, state changes, connections, errors
  - Client console: Game view, decisions, actions, observations
- **Reliability Testing**:
  - Comprehensive test suite covering all edge cases
  - Stress testing framework
  - Security validation tests

**Success Criteria for MVP**: Server is battle-tested and proven reliable before any expansion.

### Growth Features (Post-MVP)

Only after extensive testing validates MVP reliability:

- **Multiple Tables**: Support concurrent tables with independent game states
- **More Game Types**: Beyond heads-up (6-max, 9-max tables)
- **Financial System**: Deposits and withdrawals (transitioning from play money)
- **Backend Administration**: Admin tools for table management, user management, monitoring
- **Zero-Knowledge Proof Integration**: Begin mental poker protocol research and implementation

### Vision (Future)

**Mental Poker Protocol**: Evolve from traditional server architecture to a decentralized mental poker protocol using zero-knowledge proofs, enabling trustless, cryptographically secure poker games.

## User Journeys

**Journey 1: Riddler - The Architect Seeking Confidence**

Riddler has a vision: a mental poker protocol using zero-knowledge proofs that will revolutionize online poker. But first, he needs a rock-solid foundation. Late one evening, after sketching out the architecture for his poker server, he realizes the critical path: prove that a traditional server can handle every edge case flawlessly before attempting the quantum leap to ZKP-based mental poker.

The next morning, Riddler starts the cppsim server with two bot clients. Terminal windows fill his screen - the server logs on the left, two bot clients on the right. He watches as the bots connect, receive their 100 BB starting stacks, and begin playing. Hand after hand, the logs scroll by. Every action is traced: "Bot_Alpha folds," "Bot_Beta raises to 6 BB," "Server: dealing flop [Kh, 9d, 3c]." The dual-perspective logging is working beautifully - he can see both what the server thinks happened AND what each bot perceived.

Then comes the first test: Riddler terminates Bot_Alpha mid-hand during the turn. The server logs show: "Player timeout detected. Folding hand. Awarding pot to Bot_Beta." Clean. Graceful. No crash. Bot_Alpha reconnects seconds later, requests a top-up back to 100 BB, and resumes playing. "This is what I needed to see," Riddler thinks.

Over the following days, Riddler crafts increasingly malicious test scenarios. He modifies a bot to send invalid bet amounts. The server rejects them: "Invalid action: bet exceeds stack. Request denied." He simulates network failures during all-in situations. The server maintains game state integrity. He runs the simulation for 72 hours straight - 15,000 hands played without a single crash or rule violation.

The breakthrough comes when Riddler can trace any random hand through the complete dual-perspective logs and verify every decision was correct according to NLHE rules. At that moment, he knows: the foundation is solid. He's ready to build multi-table support, and eventually, his mental poker dream.

---

**Journey 2: Bot_Charlie - The Persistent Poker Player**

Bot_Charlie is spawned at 3:47 AM with a simple mission: connect to the poker server, play poker with random decisions, and simulate human-like timing. Charlie's first action is establishing a TCP connection to localhost:8080. The server responds: "Welcome! Assigned seat 1. Starting stack: 100 BB." Charlie's console logs: "Connection established. Received 100 BB. Table has 1/2 seats occupied. Waiting for second player..."

Thirty seconds later, Bot_Delta joins. The server broadcasts: "Table full. Starting hand #1." Charlie receives hole cards [As, Kh]. The server requests action: "You are the button. Post small blind (0.5 BB)?" Charlie posts the blind and waits. Delta posts the big blind. Action back to Charlie: "Call 0.5 BB or Raise?" Charlie's random strategy generator decides: raise to 3 BB. Charlie waits 1.8 seconds (human-like delay) before sending the action.

The hand plays out. Charlie wins a 6 BB pot. Charlie's stack: 103 BB. Hand #2 begins. Over the next hour, Charlie plays 47 hands. Sometimes Charlie wins, sometimes loses. By hand #34, Charlie's stack has dwindled to 12 BB. Charlie goes all-in with pocket sevens. Delta calls with ace-king. The board runs out: [Ah, 9s, 4d, 2c, Qh]. Delta wins. Charlie's stack: 0 BB.

Charlie's code detects the bust-out condition. Console log: "Stack depleted. Requesting reload to 100 BB." The request is sent. Server responds: "Reload granted. New stack: 100 BB." Charlie is back in action within 2 seconds, ready to continue the eternal battle.

This cycle repeats hundreds of times. Charlie disconnects unexpectedly during hand #103 (simulated network failure). When Charlie reconnects 5 seconds later, the server has already folded Charlie's hand and awarded the pot to Delta. Charlie's stack is intact, minus the blinds that were posted. "Clean recovery," Charlie's logs note. The game continues.

---

**Journey 3: Alex - The Test Engineer Hunting Edge Cases**

Alex is brought onto the cppsim project with one job: break it. Armed with a comprehensive test specification listing every poker scenario, network edge case, and security vulnerability, Alex sets out to prove whether this server is truly bulletproof or just another optimistic project.

Alex starts systematically. Test Case 1: "Both players go all-in pre-flop." Alex runs the scenario - server handles it perfectly, pot awarded correctly based on hand evaluation. Test Case 2: "Player disconnects during betting round." Clean timeout, hand folded, pot awarded. Test Case 3: "Player sends bet larger than stack." Server rejects the action with clear error message.

Then Alex gets creative. What if a malicious client tries to act out of turn? Alex modifies a bot to send actions when it's not their turn. Server response: "Invalid action: not your turn. Request ignored." What if a client claims to have different hole cards than the server dealt? Alex sends a malformed message. Server rejects it without crashing.

The stress test is where Alex expects to find cracks. Alex spawns a script that creates rapid connection/disconnection cycles while two bots play continuously. The server maintains stability. Alex runs a 10,000-hand simulation overnight. Morning review: zero crashes, zero rule violations, zero game state corruption.

The final test: Alex reviews random hand logs from the 10,000-hand run. Picking hand #3,847 at random, Alex traces it through both server and client logs. Every action matches. Every bet is validated. Every pot is calculated correctly. The dual-perspective logging tells the complete story.

Alex writes in the test report: "After 47 edge case tests and 10,000+ hands of stress testing, I cannot break this server. All security validations passed. All edge cases handled gracefully. Recommendation: Approved for multi-table development." Riddler can proceed with confidence.

---

**Journey 4: The Server System - Orchestrating the Perfect Hand**

The cppsim server boots up at 11:22 PM. Internal state initialization: game engine loaded, hand evaluator ready, connection listener bound to port 8080, logging system active. State: WAITING_FOR_PLAYERS.

At 11:23, a connection request arrives. The server validates the connection, assigns the client to Seat 1, allocates 100 BB to the client's stack, and logs: "Player 1 connected. Table: 1/2 occupied." State: WAITING_FOR_PLAYERS.

At 11:24, a second connection arrives. Validation passes. Seat 2 assigned. 100 BB allocated. Log: "Player 2 connected. Table: 2/2 occupied. Initiating game." State transition: WAITING_FOR_PLAYERS → IN_HAND. The server assigns the dealer button, determines blinds (Player 1: SB, Player 2: BB), and initiates the hand.

The server deals hole cards from a shuffled deck: Player 1 receives [Qd, Js], Player 2 receives [Ah, Kc]. These assignments are logged and sent to respective clients. The server posts blinds: 0.5 BB from Player 1, 1 BB from Player 2. Pot: 1.5 BB.

Action request sent to Player 1: "Call 0.5 BB, Raise, or Fold?" The server starts a 30-second timeout timer. At 2.3 seconds, Player 1's response arrives: "Raise to 3 BB." The server validates: Does Player 1 have 3 BB? Yes (100 BB stack). Is this a legal raise size? Yes (minimum raise rules met). The server processes the action: deduct 3 BB from Player 1's stack, add to pot. Pot: 4.5 BB. Log: "Player 1 raises to 3 BB."

Action request sent to Player 2: "Call 2 BB, Raise, or Fold?" Timeout timer starts. Player 2 responds: "Call 2 BB." Validation passes. Pot: 6 BB. State transition: PREFLOP_BETTING → FLOP.

The server deals the flop: [Ks, 9h, 3c]. Flop is logged and broadcast to both clients. Betting round initiates. Player 2 checks. Player 1 bets 4 BB. Player 2 folds.

The server awards the pot: 6 BB to Player 1. Player 1 new stack: 103 BB. Player 2 new stack: 97 BB. Hand complete. State transition: FLOP → HAND_COMPLETE → IN_HAND (new hand begins).

Throughout this entire sequence, every decision is validated, every state transition is logged, every edge case (timeouts, invalid actions, disconnections) has a graceful handling path. The server's mission: orchestrate perfect poker, indefinitely, reliably.

### Journey Requirements Summary

These journeys reveal the following capability requirements:

**Server Core Capabilities:**
- TCP/IP connection management (accept, validate, assign seats)
- Player stack management (initialization, updates, top-ups)
- Game state machine (waiting, dealing, betting rounds, hand completion)
- NLHE rules engine (blinds, betting validation, hand evaluation, pot calculation)
- Timeout detection and enforcement
- Disconnect/reconnect handling
- Comprehensive dual-perspective logging

**Network & Security:**
- Input validation for all client messages
- Protection against malicious/malformed messages
- Graceful degradation on network failures
- State recovery after disconnections

**Testing & Validation:**
- Comprehensive test suite covering all scenarios
- Stress testing framework (multi-hour, multi-thousand-hand runs)
- Log traceability for any hand verification
- Edge case simulation (timeouts, disconnects, invalid actions)

- Connection establishment and authentication
- Action decision engine (random strategy)
- Variable timing simulation (human-like delays)
- Auto-reload on bust-out
- Client-side comprehensive logging
- Disconnect/reconnect resilience

## Innovation & Novel Patterns

### Detected Innovation Areas

**cppsim** represents the foundation of a revolutionary vision: **the first production-ready mental poker protocol for online gaming**, using zero-knowledge proofs to create trustless, cryptographically secure poker games.

**The Innovation:**
No online poker server in production currently implements mental poker. The entire industry operates on a centralized trust model where players must trust the server to:
- Deal cards fairly
- Not reveal information to other players
- Maintain game integrity

Mental poker, first conceived in academic research in the 1980s, proposes a fundamentally different model where **cryptographic protocols replace trust**. Players can verify fairness through mathematical proofs rather than trusting a central authority.

**The Stepped Evolution Approach:**
Rather than attempting to build an unproven ZKP poker protocol from scratch, this project takes a deliberate, validation-focused approach:

1. **Traditional Server Foundation (Current MVP)**: Build and battle-test a traditional poker server to absolute reliability, proving mastery of game mechanics, networking, state management, and edge case handling
   
2. **Architecture Learning**: Extract lessons and patterns from the proven traditional server that will inform the mental poker protocol design
   
3. **Mental Poker Protocol (Future Vision)**: Evolve to a ZKP-based protocol where:
   - No central authority holds the complete deck
   - Card dealing uses cryptographic commitments and zero-knowledge proofs
   - Players can verify game fairness without trusting any party
   - Trustless gameplay becomes the standard, not the exception

### Market Context & Competitive Landscape

**Current State:**
- **All major online poker platforms** (PokerStars, 888poker, partypoker, etc.) use centralized trust models
- **Academic mental poker research** exists but has never been productionized for commercial online poker
- **Trust assumptions** are the foundation of the entire online poker industry
- **Player concerns** about fairness and collusion persist despite regulatory oversight

**The Gap:**
Despite decades of research into mental poker protocols, **zero commercial implementations exist**. The reasons include:
- **Performance concerns**: Can ZKP protocols handle real-time gameplay?
- **Complexity**: Mental poker is significantly more complex to implement than traditional servers
- **Unproven reliability**: No one has battle-tested a mental poker system at scale

**Your Unique Approach:**
You're solving the "how do we get there" problem by:
- Not skipping the foundation: Prove you can build a reliable poker server first
- Learning before leap: Understanding what works in traditional architecture before the ZKP transition
- Validation-first mindset: Extensive testing at each phase before advancing

### Validation Approach

**Phase 1 (Traditional Server) Validation:**
- Comprehensive edge case testing (all poker scenarios, network failures, malicious clients)
- Extended stress testing (thousands of hands, multi-hour runs)
- Security validation (input validation, exploit resistance)
- Success metric: Zero crashes, zero rule violations, zero exploits

**Phase 2 (Mental Poker Protocol) Validation:**
- **Cryptographic security proofs**: Mathematical verification of protocol security
- **Performance benchmarking**: Real-time gameplay latency requirements must be met
- **Security audit**: Third-party cryptographic review of the protocol
- **Adversarial testing**: Attempt to break fairness guarantees through protocol manipulation
- **Hybrid testing**: Compare mental poker hands vs. traditional server hands for outcome parity

### Risk Mitigation

**Primary Innovation Risk: ZKP Performance**
- **Risk**: Zero-knowledge proofs may not perform fast enough for real-time poker gameplay
- **Mitigation**: Build traditional server first to validate all non-cryptographic components
- **Fallback**: If pure mental poker doesn't meet performance requirements, explore hybrid models (e.g., ZKP for critical operations, optimized protocols for high-frequency actions)

**Secondary Risk: Complexity Barrier**
- **Risk**: Mental poker protocol may be too complex to implement reliably
- **Mitigation**: Learn from traditional server implementation what edge cases and patterns must be handled
- **Fallback**: Proven traditional server can operate as commercial product while mental poker research continues

**Market Risk: Adoption**
- **Risk**: Players may not understand or value the trustless guarantee
- **Mitigation**: Traditional server can establish market presence and user base before introducing mental poker as premium feature
- **Advantage**: Being first-to-market with production mental poker creates massive competitive moat

## Backend Server Specific Requirements

### Project-Type Overview

**cppsim** is a C++ network server implementing a WebSocket-based poker game protocol. The architecture prioritizes simplicity and correctness through single-threaded design, with WebSocket+JSON chosen to support future browser-based clients while maintaining clear protocol semantics for the MVP's C++ bot clients.

### Network Protocol & Communication

**Protocol Choice: WebSocket + JSON**
- **Transport**: WebSocket over TCP (supports both native C++ clients and future browser clients)
- **Serialization**: JSON for all messages (human-readable logs, easy debugging, browser compatibility)
- **Connection Management**: Single persistent WebSocket connection per client
- **Future-Proofing**: Protocol design accommodates browser clients without code changes

**Message Structure:**
- **Message Types**: ACTION, STATE_UPDATE, ERROR, HANDSHAKE, RELOAD_REQUEST, DISCONNECT
- **Protocol Versioning**: Handshake includes protocol version (e.g., "v1.0") for future compatibility
- **Idempotency**: Action messages include sequence numbers to prevent duplicate processing
- **Schema Validation**: Strict JSON schema for all message types

### Authentication & Security

**Authentication Model:**
- **MVP Approach**: Simple connection handshake with client identifier/name
- **No Persistence**: No passwords, accounts, or authentication tokens needed for play money
- **Connection Validation**: Server assigns unique session ID on successful handshake
- **Future Extension**: Architecture supports adding authentication layer for real-money post-MVP

**Message Validation & Security:**
- **Authoritative Server**: Server maintains single source of truth for all game state
- **Input Validation**: All client actions validated against current game state before processing
- **JSON Schema Enforcement**: Malformed messages rejected immediately
- **Action Validation**: 
  - Player can only act when it's their turn
  - Bet amounts must be within legal range (stack size, minimum raise)
  - Actions must be valid for current game phase
- **Exploit Prevention**: Server never trusts client-reported state (cards, stacks, pot)

**Rate Limiting:**
- **Per-Client Action Rate Limit**: Maximum 1 action per 100ms (prevents spam/DoS)
- **Connection Rate Limit**: Maximum 1 connection attempt per second per IP (prevents connection spam)
- **Invalid Action Threshold**: Disconnect client after 10 consecutive invalid actions (malicious behavior)

### Game State & Data Management

**Architectural Pattern: Single-Threaded Event Loop**
- **Concurrency Model**: Single-threaded with async I/O (Boost.Asio or similar)
- **Benefits**: No race conditions, no locks needed, deterministic execution, easier debugging
- **Event Processing**: WebSocket events processed sequentially from event queue
- **Non-Blocking I/O**: All network operations are asynchronous

**State Management:**
- **Immutable Hand History**: Each hand's complete history logged for audit trail
- **State Transitions**: Clear state machine for game phases with validation
- **Pot Calculation**: Support for main pot and side pots (all-in scenarios)
- **Stack Tracking**: Real-time stack updates with transaction-like consistency

**Serialization:**
- **Format**: JSON for all state updates (matches WebSocket protocol)
- **Logging**: All state changes serialized to JSON for dual-perspective logs
- **Efficiency**: Minimal serialization - only send state deltas when possible

### Concurrency & Performance

**Threading Model:**
- **Single Thread**: All game logic runs on single thread
- **Async I/O**: Boost.Asio or libuv for non-blocking WebSocket I/O
- **Event Loop**: Process WebSocket events, timers, and game logic sequentially

**Performance Targets:**
- **Action Processing**: < 50ms server-side processing for any valid action
- **State Broadcast**: < 100ms to serialize and send state updates to both clients  
- **Timeout Detection**: 30-second timeout detection with < 1 second granularity
- **Reconnection Window**: 30-second grace period to reconnect without losing seat

**Scalability Considerations (Post-MVP):**
- Current single-threaded design supports 1 table (2 players)
- Multi-table support will use process-per-table or thread-per-table model
- Each table remains single-threaded for simplicity
- Load balancer distributes connections to table processes

### Error Handling & Logging

**Error Categories:**
- **Connection Errors**: Disconnect, timeout, handshake failure
- **Protocol Errors**: Malformed JSON, invalid message type, schema violation
- **Game Logic Errors**: Invalid action, out-of-turn action, insufficient stack
- **System Errors**: Memory allocation failure, file I/O error

**Server-Side Logging (Console):**
- **Connection Events**: Client connect, disconnect, reconnect
- **Game Actions**: Every action with timestamp, player, action type, amount
- **State Transitions**: Hand start, phase changes, hand completion
- **Errors**: All validation failures and error conditions
- **Performance Metrics**: Action processing time, state update latency

**Client-Side Logging (Console):**
- **Game View**: Cards received, pot size, valid actions
- **Decisions**: Action chosen, reasoning (for bot strategy)
- **State Updates**: All game state changes from client perspective
- **Observations**: What client sees vs. what server broadcasts

### Implementation Considerations

**Technology Stack:**
- **Language**: C++17 or later
- **WebSocket Library**: Boost.Beast or uWebSockets
- **JSON Library**: nlohmann/json or RapidJSON
- **Async I/O**: Boost.Asio
- **Build System**: CMake for cross-platform builds
- **Testing Framework**: Google Test for unit tests

**Development Priorities:**
1. **Correctness First**: Prove all poker rules implemented correctly
2. **Observability**: Comprehensive logging before optimization
3. **Testability**: Unit tests for all game logic, integration tests for protocol
4. **Simplicity**: Single-threaded design reduces complexity for MVP validation

## Project Scoping & Phased Development

### MVP Strategy & Philosophy

**MVP Approach:** Platform MVP with Validation-First Philosophy

This project deliberately prioritizes **foundation quality over feature velocity**. The MVP is not about delivering minimum features quickly - it's about proving the traditional poker server architecture is bulletproof before attempting the revolutionary leap to mental poker using zero-knowledge proofs.

**Strategic Rationale:**
- **No Timeline Pressure**: Extensive testing takes priority over launch dates
- **Learn Before Leap**: Extract architectural lessons from traditional server before ZKP implementation
- **De-Risk Innovation**: Validate all non-cryptographic components before adding cryptographic complexity
- **Confidence to Scale**: Prove single-table reliability before multi-table or mental poker expansion

**Resource Requirements:**
- **Team**: 1-2 developers (one can build and test MVP)
- **Skills**: C++ expertise, network programming, poker domain knowledge
- **Timeline**: No fixed deadline - completion when reliability is proven through extensive testing

### MVP Feature Set (Phase 1)

**Core Objective:** Prove a traditional poker server can handle all edge cases flawlessly

**Must-Have Capabilities:**
- **Single Heads-Up Table**: Exactly 2 players, one table
- **Complete NLHE Rules**: Full poker logic (blinds, betting, hand evaluation, pot management with side pots)
- **Play Money Economy**: 100 BB starting stacks, auto top-up, no persistence
- **WebSocket+JSON Protocol**: Single-threaded server with async I/O
- **Network Resilience**: Graceful disconnect/timeout handling, reconnection support
- **Security & Validation**: All client messages validated, malicious clients blocked
- **Bot Clients**: C++ bots with random strategies, variable timing, auto-reload
- **Dual-Perspective Logging**: Complete server + client console logs
- **Comprehensive Testing**: Edge case tests, stress tests (10,000+ hands), security validation

**MVP Success Criteria:**
- Zero crashes during extended stress testing
- Zero rule violations across thousands of hands
- Zero exploits in security validation
- Complete log traceability for any hand
- Confidence to proceed to multi-table development

### Post-MVP Features

**Phase 2: Multi-Table & Real Money (Post-Validation)**

Only after extensive MVP testing validates single-table reliability:

- **Multiple Concurrent Tables**: Process-per-table or thread-per-table architecture
- **More Game Types**: 6-max, 9-max tables beyond heads-up
- **Financial System**: Deposits, withdrawals, real money handling (transition from play money)
- **Backend Administration**: Table management, user management, monitoring tools
- **Enhanced Security**: Authentication, authorization, audit logs for real money

**Phase 3: Mental Poker Protocol (Vision)**

The revolutionary transition to trustless poker:

- **Zero-Knowledge Proof Integration**: Research and implement ZKP-based mental poker
- **Cryptographic Card Dealing**: No central authority holds deck
- **Trustless Verification**: Players verify fairness through mathematical proofs
- **Protocol Performance Optimization**: Meet real-time gameplay requirements with ZKP overhead
- **Security Audits**: Third-party cryptographic review of mental poker protocol

### Risk Mitigation Strategy

**Technical Risks:**
- **Risk**: Single-threaded design may not scale to multiple tables
- **Mitigation**: Design allows evolution to multi-process/multi-thread model for Phase 2
- **Validation**: Stress test single table to understand performance characteristics

**Innovation Risks:**
- **Risk**: ZKP mental poker may not perform well enough for real-time gameplay
- **Mitigation**: Build robust traditional server first; can operate commercially while ZKP research continues
- **Fallback**: Hybrid approach (ZKP for critical operations, optimized protocols for high-frequency actions)

**Market Risks:**
- **Risk**: Players may not understand or value trustless mental poker
- **Mitigation**: Establish user base with proven traditional server before introducing mental poker
- **Advantage**: First-to-market with mental poker creates massive competitive moat

**Resource Risks:**
- **Risk**: Underestimating complexity of bulletproof poker server
- **Mitigation**: No timeline pressure allows thorough testing and validation
- **Contingency**: Single-table MVP can launch as niche product (heads-up poker simulator) if expansion delayed

### Development Philosophy

**Quality Gates:**
Each phase must pass strict validation before proceeding:

1. **MVP Completion**: Only when zero crashes, zero exploits, zero rule violations proven
2. **Multi-Table Ready**: Only when MVP architecture lessons extracted and applied
3. **Mental Poker Launch**: Only when cryptographic security proven and performance validated

**No Compromises:**
- Security validation failures block launch
- Edge case bugs block phase progression
- Performance below targets triggers architecture review

This is **not** a minimum viable product in the traditional sense - it's a **maximum reliability foundation** for future innovation.

## Functional Requirements

### Connection & Session Management

- **FR1**: Server can accept WebSocket connections from poker clients
- **FR2**: Server can validate client handshake and assign unique session IDs
- **FR3**: Server can detect when a table has 2 connected players and start the game
- **FR4**: Server can detect client disconnections and timeouts during gameplay
- **FR5**: Server can allow disconnected clients to reconnect within a grace period without losing their seat
- **FR6**: Server can remove timed-out clients from the table and award pots accordingly
- **FR7**: Server can inform waiting clients when the table is full (more than 2 connection attempts)

### Poker Game Management

- **FR8**: Server can initialize a heads-up NLHE poker game with 2 players
- **FR9**: Server can assign dealer button and blind positions (small blind, big blind)
- **FR10**: Server can shuffle and deal hole cards to each player
- **FR11**: Server can manage betting rounds (preflop, flop, turn, river)
- **FR12**: Server can deal community cards at appropriate betting rounds
- **FR13**: Server can determine hand winners using standard NLHE hand rankings
- **FR14**: Server can calculate and award pots (including side pots for all-in scenarios)
- **FR15**: Server can transition between game phases following poker rules
- **FR16**: Server can start a new hand automatically after the previous hand completes

### Player Actions & Betting

- **FR17**: Players can post blinds (small blind, big blind) when required  
- **FR18**: Players can fold their hand
- **FR19**: Players can call the current bet
- **FR20**: Players can raise the bet (subject to min/max raise rules)
- **FR21**: Players can check when no bet is required
- **FR22**: Players can go all-in with their remaining stack
- **FR23**: Server can enforce action ordering (players must act in turn)
- **FR24**: Server can enforce bet sizing rules (minimum raise, maximum bet up to stack)
- **FR25**: Server can enforce 30-second action timeouts and auto-fold on timeout

### Chip & Stack Management

- **FR26**: Server can assign 100 BB starting stack to each connecting player
- **FR27**: Players can request chip reload when stack falls below 100 BB
- **FR28**: Server can grant chip reloads up to 100 BB (play money, unlimited)
- **FR29**: Server can track player stack balances in real-time
- **FR30**: Server can deduct bets from player stacks
- **FR31**: Server can add won pots to player stacks

### Network & Communication

- **FR32**: Server can send game state updates to clients as JSON messages over WebSocket
- **FR33**: Server can receive player actions from clients as JSON messages over WebSocket
- **FR34**: Server can broadcast hand results to all players
- **FR35**: Server can inform players of valid actions available to them
- **FR36**: Server can send error messages to clients for invalid actions
- **FR37**: Server can support protocol versioning in handshake messages

### Security & Validation

- **FR38**: Server can validate all incoming client messages against JSON schema
- **FR39**: Server can reject malformed or invalid client messages
- **FR40**: Server can validate that player actions are legal given current game state
- **FR41**: Server can prevent players from acting out of turn
- **FR42**: Server can prevent players from betting more than their stack
- **FR43**: Server can enforce rate limiting (max 1 action per 100ms per client)
- **FR44**: Server can disconnect clients that send excessive invalid actions (> 10 consecutive)
- **FR45**: Server can maintain authoritative game state (never trust client-reported data)

### Logging & Observability

- **FR46**: Server can log all connection events (connect, disconnect, reconnect) to console
- **FR47**: Server can log all game actions (folds, calls, raises) with timestamps to console
- **FR48**: Server can log all state transitions (hand start, betting round changes, hand completion) to console
- **FR49**: Server can log all errors and validation failures to console
- **FR50**: Server can log performance metrics (action processing time) to console
- **FR51**: Bot clients can log their hole cards and game view to console
- **FR52**: Bot clients can log their action decisions and reasoning to console
- **FR53**: Bot clients can log all game state updates they receive to console

### Bot Client Behavior

- **FR54**: Bot clients can connect to the server via WebSocket
- **FR55**: Bot clients can make random poker decisions (fold, call, raise)
- **FR56**: Bot clients can simulate variable response times (1-3 seconds) before acting
- **FR57**: Bot clients can automatically request chip reloads when busted
- **FR58**: Bot clients can handle disconnections and attempt reconnection
- **FR59**: Bot clients can parse and respond to server game state updates

### Testing & Validation

- **FR60**: System can run comprehensive edge case tests (all-in scenarios, disconnections during betting, timeouts, etc.)
- **FR61**: System can run extended stress tests (10,000+ hands without crashes)
- **FR62**: System can run security validation tests (invalid messages, out-of-turn actions, malicious clients)
- **FR63**: System can verify log traceability (any hand can be reconstructed from logs)
- **FR64**: System can measure and validate performance targets (< 50ms action processing, < 100ms state updates)

## Non-Functional Requirements

### Performance

- **NFR-P1**: Server action processing must complete within 50ms for any valid poker action
- **NFR-P2**: Server state update broadcasts must complete within 100ms total (serialization + network transmission)
- **NFR-P3**: Timeout detection must have < 1 second granularity for 30-second action timeouts
- **NFR-P4**: Reconnection grace period must support 30 seconds without performance degradation
- **NFR-P5**: Bot client simulated response times must range from 1-3 seconds to simulate human-like play
- **NFR-P6**: JSON serialization/deserialization must not introduce measurable latency (< 5ms per operation)

### Reliability

- **NFR-R1**: Server must operate for extended periods (72+ hours) without crashes
- **NFR-R2**: Server must handle 10,000+ consecutive poker hands without failures or memory leaks
- **NFR-R3**: Server must gracefully handle client disconnections without corrupting game state
- **NFR-R4**: Server must maintain game state consistency across all error conditions
- **NFR-R5**: Bot clients must automatically reconnect after network failures without manual intervention
- **NFR-R6**: System must achieve zero rule violations across thousands of hands
- **NFR-R7**: All state transitions must be deterministic and reproducible

### Security

- **NFR-S1**: Server must validate 100% of incoming client messages against JSON schema
- **NFR-S2**: Server must reject all malformed or invalid client messages without crashing
- **NFR-S3**: Server must prevent any client action that violates poker rules or game state
- **NFR-S4**: Server must maintain authoritative game state (zero trust of client-reported data)
- **NFR-S5**: Server must enforce rate limiting (max 1 action per 100ms per client)
- **NFR-S6**: Server must disconnect clients exhibiting malicious behavior (> 10 consecutive invalid actions)
- **NFR-S7**: Server must achieve zero exploitable vulnerabilities in security validation testing
- **NFR-S8**: Server must prevent out-of-turn actions, illegal bet sizes, and stack manipulation attempts

### Testability & Observability

- **NFR-T1**: All game actions must be logged with sufficient detail to reconstruct any hand from logs alone
- **NFR-T2**: Server logs must capture all connection events, state transitions, errors, and performance metrics
- **NFR-T3**: Bot client logs must provide dual-perspective view (what client sees vs. what server broadcasts)
- **NFR-T4**: System must support comprehensive edge case testing (all-in scenarios, disconnections, timeouts)
- **NFR-T5**: System must support extended stress testing (10,000+ hands) with full observability
- **NFR-T6**: System must provide log traceability to verify correctness of any random hand
- **NFR-T7**: Performance metrics (action processing time, state update latency) must be logged for validation

### Maintainability

- **NFR-M1**: Codebase must use clear separation between poker game logic, network protocol, and client simulation
- **NFR-M2**: Single-threaded design must be maintained to ensure deterministic execution and easier debugging
- **NFR-M3**: Architecture must support evolution to multi-table (process-per-table or thread-per-table) without core rewrites
- **NFR-M4**: Protocol design must support versioning for future compatibility
- **NFR-M5**: Code must achieve comprehensive unit test coverage for all poker game logic
- **NFR-M6**: Build system (CMake) must support cross-platform builds without platform-specific hacks
