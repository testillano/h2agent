function(set_cmake_os_type)

  # Detect operating system
  message(STATUS "This is a ${CMAKE_SYSTEM_NAME} system")
  if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_definitions(-DSYSTEM_LINUX)
  endif()
  if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    add_definitions(-DSYSTEM_DARWIN)
  endif()
  if(${CMAKE_SYSTEM_NAME} STREQUAL "AIX")
    add_definitions(-DSYSTEM_AIX)
  endif()

  # Detect host processor
  message(STATUS "The host processor is ${CMAKE_HOST_SYSTEM_PROCESSOR}")
  message(STATUS "The c compiler is ${CMAKE_C_COMPILER}")
  message(STATUS "The c++ compiler is ${CMAKE_CXX_COMPILER}")
  message(STATUS "The build type is ${CMAKE_BUILD_TYPE}")

endfunction()
