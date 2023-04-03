add_library (h2agent-http2
    ${CMAKE_CURRENT_LIST_DIR}/MyTrafficHttp2Server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MyAdminHttp2Server.cpp
)

target_include_directories (h2agent-http2
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/http2
    ${CMAKE_SOURCE_DIR}/src/jsonSchema
    ${CMAKE_SOURCE_DIR}/src/model

INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

