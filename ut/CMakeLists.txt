add_executable( unit-test "")


# To avoid setting absolute paths for libraries down at 'target_link_libraries':
add_library(ert_logger STATIC IMPORTED)
add_library(ert_http2comm STATIC IMPORTED)
add_library(ert_metrics STATIC IMPORTED)
set_property(TARGET ert_logger PROPERTY IMPORTED_LOCATION /usr/local/lib/ert/libert_logger.a)
set_property(TARGET ert_http2comm PROPERTY IMPORTED_LOCATION /usr/local/lib/ert/libert_http2comm.a)
set_property(TARGET ert_metrics PROPERTY IMPORTED_LOCATION /usr/local/lib/ert/libert_metrics.a)

target_link_libraries( unit-test
PRIVATE
-static
        h2agent-http2Server
        h2agent-jsonSchema
        h2agent-model
        ert_logger
        ert_http2comm
        ert_metrics

        boost_system   #Needed by nghttp2_asio
        nghttp2_asio   #Needed by nghttp2
        nghttp2
        pthread        #Needed by boost::asio
        ssl            #Needed by boost_system
        crypto         #Needed by ssl, and need to be appended after ssl
        dl             #Needed by crypto

        prometheus-cpp-pull
        prometheus-cpp-core
        z

        nlohmann_json_schema_validator

        #boost_thread
        #boost_filesystem
        #boost_system
        gtest
        gmock
        gtest_main
)

add_subdirectory(jsonSchema)
add_subdirectory(http2Server)
