add_executable( h2client main.cpp )

add_library(ert_logger STATIC IMPORTED)
add_library(ert_http2comm STATIC IMPORTED)
add_library(ert_metrics STATIC IMPORTED)
add_library(prometheus-cpp-pull STATIC IMPORTED)
add_library(prometheus-cpp-core STATIC IMPORTED)
add_library(boost_system STATIC IMPORTED)
add_library(nghttp2_asio STATIC IMPORTED)
add_library(nghttp2 STATIC IMPORTED)

set_property(TARGET ert_logger PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/ert/libert_logger.a)
set_property(TARGET ert_http2comm PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/ert/libert_http2comm.a)
set_property(TARGET ert_metrics PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/ert/libert_metrics.a)
set_property(TARGET prometheus-cpp-pull PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/libprometheus-cpp-pull.a)
set_property(TARGET prometheus-cpp-core PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/libprometheus-cpp-core.a)
set_property(TARGET boost_system PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/libboost_system.a)
set_property(TARGET nghttp2_asio PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/libnghttp2_asio.a)
set_property(TARGET nghttp2 PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/libnghttp2.a)

target_link_libraries( h2client
PRIVATE
${CMAKE_EXE_LINKER_FLAGS}
        ert_http2comm
        ert_logger
        ert_metrics

        prometheus-cpp-pull
        prometheus-cpp-core
        z

        boost_system   #Needed by nghttp2_asio
        nghttp2_asio   #Needed by nghttp2
        nghttp2
        ssl            #Needed by boost_system
        crypto         #Needed by ssl, and need to be appended after ssl
        pthread        #Needed by boost::asio
        #dl             #Needed by crypto

        ) # target_link_libraries

