############
# Versions #
############
cmake_minimum_required (VERSION 3.5.1)
project(h2agent LANGUAGES CXX)

set(MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MAIN_PROJECT ON)
endif()

#############
# Variables #
#############
set(H2AGENT_TARGET_NAME       ${PROJECT_NAME})

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Git version
EXECUTE_PROCESS(
  COMMAND git describe --long
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE "GIT_VERSION"
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)
MESSAGE( STATUS "Git version: ${GIT_VERSION}" )

###########
# Modules #
###########
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/")

# Operative system
include(os_type)
set_cmake_os_type()

# Build type
include(build_type)
set_cmake_build_type()

# Static linking
include (static_linking)
set_cmake_static_linking()

# Build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/lib)
message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY is ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_LIBRARY_OUTPUT_DIRECTORY is ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_ARCHIVE_OUTPUT_DIRECTORY is ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

# Compilation flags
include(compiler_flags)
set_cmake_compiler_flags()

# CMAKE_PREFIX_PATH
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH /usr/local)
endif()

################
# Dependencies #
################
#include(FetchContent)
#
## nlohmann json header-only library (https://github.com/nlohmann/json.git):
## We will point to a lighter repository to speed up the process:
#FetchContent_Declare(nlohmann_json
#  GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
#  GIT_TAG v3.10.5)
#message(STATUS  "Fetching nlohmann_json ... this may take a while")
#FetchContent_GetProperties(nlohmann_json)
#
#if(NOT nlohmann_json_POPULATED)
#  FetchContent_Populate(nlohmann_json)
#  add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR} EXCLUDE_FROM_ALL)
#endif()
#
## pboettch json-schema-validator header-only library:
#FetchContent_Declare(pboettch_jsonschemavalidator
#  GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
#  GIT_TAG 2.1.0)
#message(STATUS  "Fetching pboettch_jsonschemavalidator ... this may take a while")
#FetchContent_GetProperties(pboettch_jsonschemavalidator)
#
#if(NOT pboettch_jsonschemavalidator_POPULATED)
#  FetchContent_Populate(pboettch_jsonschemavalidator)
#  add_subdirectory(${pboettch_jsonschemavalidator_SOURCE_DIR} ${pboettch_jsonschemavalidator_BINARY_DIR} EXCLUDE_FROM_ALL)
#endif()

##################
# Subdirectories #
##################
add_subdirectory( src )
add_subdirectory( ut )
add_subdirectory( tools/matching-helper )
add_subdirectory( tools/arashpartow-helper )
add_subdirectory( tools/h2client )
add_subdirectory( tools/udp-server )
add_subdirectory( tools/udp-server-h2client )

###########
# Install #
###########
#SET(MY_OWN_INSTALL_PREFIX "/opt/h2agent" CACHE PATH "Prefix prepended to install directories")
SET(CMAKE_INSTALL_PREFIX "${MY_OWN_INSTALL_PREFIX}" CACHE INTERNAL "Prefix prepended to install directories" FORCE)

include(GNUInstallDirs)
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
file(GLOB_RECURSE BINARIES ${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/bin/*)
install(PROGRAMS ${BINARIES} DESTINATION bin)

###########
# DOXYGEN #
###########
find_package(Doxygen)
IF(Doxygen_FOUND)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/docs/doxygen/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs/doxygen
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )
ENDIF(Doxygen_FOUND)

