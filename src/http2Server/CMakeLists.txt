add_library (h2agent-http2Server
    ${CMAKE_CURRENT_LIST_DIR}/functions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MyTrafficHttp2Server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MyAdminHttp2Server.cpp
)

target_include_directories (h2agent-http2Server
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/http2Server
    ${CMAKE_SOURCE_DIR}/src/jsonSchema
    ${CMAKE_SOURCE_DIR}/src/model

INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

