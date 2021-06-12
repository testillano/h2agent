add_library (h2agent-model
    ${CMAKE_CURRENT_LIST_DIR}/TypeConverter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Transformation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockRequest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockRequests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminMatchingData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminProvision.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminProvisionData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockRequestData.cpp
)

target_include_directories (h2agent-model
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/model

INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

