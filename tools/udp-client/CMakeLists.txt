add_executable( udp-client main.cpp )

add_library(ert_logger STATIC IMPORTED)

set_property(TARGET ert_logger PROPERTY IMPORTED_LOCATION ${CMAKE_PREFIX_PATH}/lib/ert/libert_logger.a)

target_link_libraries( udp-client
PRIVATE
${CMAKE_EXE_LINKER_FLAGS}
        ert_logger

        ) # target_link_libraries
