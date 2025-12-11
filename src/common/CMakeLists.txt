# Common library (shared protocol, poker rules, hand evaluator)
add_library(poker_common STATIC
  protocol.cpp
  protocol.hpp
)

# Include directories
target_include_directories(poker_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(poker_common
  PUBLIC
    nlohmann_json::nlohmann_json
    project_options
  PRIVATE
    project_warnings
)
