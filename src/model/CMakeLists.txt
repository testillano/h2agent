add_library (h2agent-model
    ${CMAKE_CURRENT_LIST_DIR}/TypeConverter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Transformation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerKeyEvent.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerKeyEvents.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerMatchingData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerProvision.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerProvisionData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminSchema.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminSchemaData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerEventsData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/GlobalVariable.cpp
)

target_include_directories (h2agent-model
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/model
    ${CMAKE_SOURCE_DIR}/src/jsonSchema
    ${CMAKE_SOURCE_DIR}/src/http2

INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

