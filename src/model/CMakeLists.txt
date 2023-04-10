add_library (h2agent-model
    ${CMAKE_CURRENT_LIST_DIR}/functions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/TypeConverter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Transformation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockEvent.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockEventsHistory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerEvent.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerEventsHistory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerMatchingData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerProvision.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerProvisionData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminClientEndpoint.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminClientEndpointData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminSchema.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminSchemaData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/GlobalVariable.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Configuration.cpp
    ${CMAKE_CURRENT_LIST_DIR}/FileManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SafeFile.cpp
    ${CMAKE_CURRENT_LIST_DIR}/DataPart.cpp
)

target_include_directories (h2agent-model
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/model
    ${CMAKE_SOURCE_DIR}/src/jsonSchema
    ${CMAKE_SOURCE_DIR}/src/http2

INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

