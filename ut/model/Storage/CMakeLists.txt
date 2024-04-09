target_sources( unit-test
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/globalVariable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/typeConverter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mockServerEvent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mockServerEventsHistory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mockServerData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mockClientEvent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mockClientEventsHistory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mockClientData.cpp
)
