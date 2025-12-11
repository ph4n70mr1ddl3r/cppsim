**🔥 CODE REVIEW FINDINGS, Riddler!**

**Story:** 1-1-cmake-project-structure-dependencies.md
**Git vs Story Discrepancies:** 3 found (Files in build/git but not in story: `tests/unit/protocol_test.cpp`, `tests/integration/websocket_server_test.cpp`, `tests/integration/handshake_test.cpp`)
**Issues Found:** 1 High, 3 Medium, 2 Low

## 🔴 CRITICAL ISSUES
- **[High] Server Accept Infinite Loop**: `src/server/websocket_server.cpp`'s `on_accept` immediately calls `do_accept` even on error. If a persistent error occurs (like file descriptor exhaustion `EMFILE`), this triggers a tight infinite loop that floods logs and spikes CPU usage. It must implement a backoff strategy or exit on fatal errors.

## 🟡 MEDIUM ISSUES
- **[Medium] Missing Test Files in Documentation**: The build system `tests/CMakeLists.txt` compiles `unit/protocol_test.cpp`, `integration/websocket_server_test.cpp`, and `integration/handshake_test.cpp`, but these files are missing from the Story 1-1 File List. Documentation must match reality.
- **[Medium] Thread-Unsafe Logging**: `src/common/protocol.cpp` writes directly to `std::cerr` without locking, while `src/server/websocket_session.cpp` uses a mutex. This inconsistency risks interleaved log output and race conditions in a multithreaded server.
- **[Medium] Incomplete Graceful Shutdown**: `websocket_server::stop()` only closes the acceptor. It fails to notify `connection_manager` to close active sessions gracefully (sending WebSocket Close frames), causing abrupt disconnects for clients.

## 🟢 LOW ISSUES
- **[Low] Unused Dependencies in Client**: `poker_client` links `Boost::beast`, `Boost::asio`, and `poker_common` in CMake, but uses none of them. This adds unnecessary build overhead and binary size.
- **[Low] Silent Protocol Version Overwrite**: `protocol::parse_handshake` in `src/common/protocol.cpp` silently overwrites `msg.protocol_version` with `envelope.protocol_version` on mismatch, potentially hiding client defects.