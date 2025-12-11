# Poker server executable
add_executable(poker_server)

target_sources(poker_server
  PRIVATE
    main.cpp
    websocket_server.cpp
    websocket_session.cpp
    connection_manager.cpp
)

# Link dependencies
target_link_libraries(poker_server
  PRIVATE
    poker_common
    Boost::beast
    Boost::asio
    Boost::system
)

# Set compile features
target_compile_features(poker_server PRIVATE cxx_std_17)
