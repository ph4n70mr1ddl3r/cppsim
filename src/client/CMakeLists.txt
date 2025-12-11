# Poker client (bot) executable
add_executable(poker_client)

target_sources(poker_client
  PRIVATE
    main.cpp
)

# Link dependencies
target_link_libraries(poker_client
  PRIVATE
    project_options
    project_warnings
)

# Set compile features
target_compile_features(poker_client PRIVATE cxx_std_17)
