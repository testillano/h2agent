add_library (h2agent-engine
    ${CMAKE_CURRENT_LIST_DIR}/Engine.cpp
)

target_include_directories (h2agent-engine
INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
