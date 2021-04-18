add_library (h2agent-model
    ${CMAKE_CURRENT_LIST_DIR}/AdminMatchingData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminProvision.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminProvisionData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockData.cpp
)

target_include_directories (h2agent-model
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/model

INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

