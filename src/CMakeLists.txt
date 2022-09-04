add_subdirectory(http2)
add_subdirectory(jsonSchema)
add_subdirectory(model)

add_executable( ${H2AGENT_TARGET_NAME}
        # Keep the list of files sorted.
        main.cpp
        version.cpp
        ) # add_executable h2agent

# To avoid setting absolute paths for libraries down at 'target_link_libraries':
add_library(ert_logger STATIC IMPORTED)
add_library(ert_http2comm STATIC IMPORTED)
add_library(ert_metrics STATIC IMPORTED)
add_library(ert_multipart STATIC IMPORTED)
set_property(TARGET ert_logger PROPERTY IMPORTED_LOCATION /usr/local/lib/ert/libert_logger.a)
set_property(TARGET ert_http2comm PROPERTY IMPORTED_LOCATION /usr/local/lib/ert/libert_http2comm.a)
set_property(TARGET ert_metrics PROPERTY IMPORTED_LOCATION /usr/local/lib/ert/libert_metrics.a)
set_property(TARGET ert_multipart PROPERTY IMPORTED_LOCATION /usr/local/lib/ert/libert_multipart.a)

# https://github.com/jgomezselles/hermes/commit/e659b86f4190598a81783016307768001a9d6fa3: -static is enough
target_link_libraries( ${H2AGENT_TARGET_NAME}
PRIVATE
${CMAKE_EXE_LINKER_FLAGS}
        h2agent-http2
        h2agent-model
        ert_logger
        ert_http2comm
        ert_metrics
        ert_multipart

        prometheus-cpp-pull
        prometheus-cpp-core
        z

        boost_system   #Needed by nghttp2_asio
        nghttp2_asio   #Needed by nghttp2
        nghttp2
        ssl            #Needed by boost_system
        crypto         #Needed by ssl, and need to be appended after ssl
        pthread        #Needed by boost::asio
        dl             #Needed by crypto

        h2agent-jsonSchema
        nlohmann_json_schema_validator

        ) # target_link_libraries

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp @ONLY)

