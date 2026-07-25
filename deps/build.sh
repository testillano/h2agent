#!/bin/sh
#
# ENTRYPOINT BUILDER SCRIPT
#
# Prepend variables:
#  MAKE_PROCS (number of concurrent make 'jobs', maximum available by default): 'make' parallel jobs.
#  BUILD_TYPE ([Release]|Debug): set the build type.
#  STATIC_LINKING ([TRUE]|FALSE): add '-static' to executables linking.
#
# Command-line arguments:
# * Having cmake system:
#   $1: extra cmake arguments; $2-$@: extra make arguments
# * Not having cmake system:
#   $@: extra make arguments

BUILD_TYPE=${BUILD_TYPE:-Release}
MAKE_PROCS=${MAKE_PROCS:-$(grep processor /proc/cpuinfo -c)}
STATIC_LINKING=${STATIC_LINKING:-TRUE}

[ -f CMakeLists.txt ] && cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DSTATIC_LINKING="${STATIC_LINKING}" "$1" . && [ -n "$1" ] && shift
make -j"${MAKE_PROCS}" $@

exit $?

