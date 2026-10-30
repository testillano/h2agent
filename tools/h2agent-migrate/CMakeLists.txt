# Converter library: shared by the tool and the unit tests (nlohmann only).
add_library( h2agent-migrate-lib GrammarMigrator.cpp )
target_include_directories( h2agent-migrate-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

add_executable( h2agent-migrate main.cpp )
target_link_libraries( h2agent-migrate PRIVATE ${CMAKE_EXE_LINKER_FLAGS} h2agent-migrate-lib )
