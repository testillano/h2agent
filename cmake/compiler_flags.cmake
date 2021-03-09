function(set_cmake_compiler_flags)

  #add_compile_options(
  #    "-Wall"
  #    "-Werror"
  #    $<$<CONFIG:Debug>:--coverage>
  #)

  #link_libraries(
  #    $<$<CONFIG:Debug>:--coverage>
  #)

  # Example how to set c++ compiler flags for GNU
  message(STATUS "CMAKE_CXX_COMPILER_ID = ${CMAKE_CXX_COMPILER_ID}")
  if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    #execute_process(COMMAND g++ --version >/dev/null 2>/dev/null)
    set(CMAKE_CXX_COMPILER             "/usr/bin/g++")
    set(CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wno-deprecated -Wwrite-strings -Wno-unknown-pragmas -Wno-sign-compare -Wno-maybe-uninitialized -Wno-unused -Wno-reorder")
    set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g3")
    set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE        "-O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang)
    #execute_process(COMMAND clang++ --version >/dev/null 2>/dev/null)
    add_definitions(-DIS_CLANG)
    set(CMAKE_CXX_COMPILER             "/usr/bin/clang++")
    set(CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wno-deprecated -Wwrite-strings -Wno-unknown-pragmas -Wno-sign-compare -Wno-maybe-uninitialized -Wno-unused -Wno-reorder -Wno-parentheses-equality")
    set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g3")
    set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE        "-O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
  endif()

endfunction()
