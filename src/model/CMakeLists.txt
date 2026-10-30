add_library (h2agent-model
    ${CMAKE_CURRENT_LIST_DIR}/functions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/TypeConverter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Transformation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/EngineGrammarV1.cpp
    ${CMAKE_CURRENT_LIST_DIR}/EngineHost.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockEvent.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockEventsHistory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerEvent.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerEventsHistory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockServerData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockClientEvent.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockClientEventsHistory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MockClientData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerMatchingData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerProvision.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminServerProvisionData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminClientProvision.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminClientProvisionData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminClientEndpoint.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminClientEndpointData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminSchema.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AdminSchemaData.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Vault.cpp
    ${CMAKE_CURRENT_LIST_DIR}/WaitManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SseManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Configuration.cpp
    ${CMAKE_CURRENT_LIST_DIR}/FileManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SafeFile.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SocketManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SafeSocket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/DataPart.cpp
)

target_include_directories (h2agent-model
PRIVATE
    ${CMAKE_SOURCE_DIR}/src/model
    ${CMAKE_SOURCE_DIR}/src/jsonSchema
    ${CMAKE_SOURCE_DIR}/src/http2
    ${CMAKE_SOURCE_DIR}/src/engine

INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)


# Grammar v2 -> v1 bridge (phase [0.7a], GrammarV2Bridge.hpp): temporary.
target_link_libraries( h2agent-model PUBLIC h2agent-migrate-lib )
