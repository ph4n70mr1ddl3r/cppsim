# Poker server library (for testing)
add_library(poker_server_lib STATIC
  websocket_server.cpp
  websocket_session.cpp
  connection_manager.cpp
  logger.cpp
  runtime_config.cpp
)

target_link_libraries(poker_server_lib
  PUBLIC
    poker_common
    Boost::headers
  PRIVATE
    project_warnings
)

target_include_directories(poker_server_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Poker server executable
add_executable(poker_server)

target_sources(poker_server
  PRIVATE
    main.cpp
)

# Link dependencies
target_link_libraries(poker_server
  PRIVATE
    poker_server_lib
)


