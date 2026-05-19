# Poker client (bot) executable
add_executable(poker_client)

target_sources(poker_client
  PRIVATE
    main.cpp
)

# Link dependencies
# project_options is inherited transitively via poker_common (PUBLIC).
# project_warnings is linked PRIVATE to each executable in the root CMakeLists.txt.
target_link_libraries(poker_client
  PRIVATE
    poker_common
)

