#!/bin/bash
# Valgrind helper

echo
project_root_dir="$(dirname "$(readlink -f "$0")")/.."

execs=( $(ls ${project_root_dir}/build/*/bin/h2agent 2>/dev/null) )
[ $? -ne 0 ] && echo -e "Warning: you should build the project before using valgrind suite (i.e.: build_type=Debug ./build.sh --auto)\n"


MEMCHECK="valgrind --show-leak-kinds=definite --leak-check=full --show-reachable=no --errors-for-leak-kinds=definite,possible --track-origins=yes --log-file=memcheck.out" # memory
CALLGRIND="valgrind --tool=callgrind" # cpu consumption
CALLGRIND_START_DISABLED="valgrind --tool=callgrind --instr-atstart=no" # cpu consumption
CALLGRIND_JUMPS="valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes" # jumps
HELGRIND="valgrind --tool=helgrind --log-file=helgrind.out" # threading errors
MASSIF="valgrind --tool=massif --time-unit=B" # heap profiler

echo "--------------------"
echo "Valgrind suite tools"
echo "--------------------"
echo
for tool in MEMCHECK CALLGRIND_START_DISABLED CALLGRIND CALLGRIND_JUMPS HELGRIND MASSIF
do
  prefix=$(eval echo \$${tool})
  echo "${tool}: ${prefix} <executable>"
done

cat << EOF

--------------
Callgrind help
--------------

Native kcachegrind (GUI for callgrind output) could not show function names for
 debug-linked executable built within docker builder image. Docker builder image
 uses musl over alpine, and probably your system uses glibc. So, you may build
 the project natively, or install kcachegrind within the docker container (you
 would need X11).

Callgrind control:
  Start instrumentation:          callgrind_control -i on
  Stop instrumentation:           callgrind_control -i off
  Reset current callgrind events: callgrind_control -z
  Dump current callgrind events:  callgrind_control --dump

EOF
