function(set_cmake_static_linking)

  # determine, whether we want a static binary
  set(STATIC_LINKING FALSE CACHE BOOL "Build a static binary ?")

  # set -static, when STATIC_LINKING is TRUE
  if(STATIC_LINKING)
    set(CMAKE_EXE_LINKER_FLAGS "-static" CACHE INTERNAL "CMAKE_EXE_LINKER_FLAGS")
  endif(STATIC_LINKING)

  message(STATUS "Static linking is ${STATIC_LINKING} (CMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS})")

endfunction()
