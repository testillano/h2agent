add_library (h2agent-jsonSchema
    ${CMAKE_CURRENT_LIST_DIR}/JsonSchema.cpp
)

target_include_directories (h2agent-jsonSchema
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/jsonSchema

INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

