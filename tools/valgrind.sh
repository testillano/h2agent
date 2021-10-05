#!/bin/bash
# Valgrind helper

echo
git_root_dir="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$git_root_dir" ] && { echo "Go into the git repository !" ; exit 1 ; }

execs=( $(ls ${git_root_dir}/build/*/bin/h2agent 2>/dev/null) )
[ $? -ne 0 ] && echo "Build the project before (i.e. ./build.sh --auto)" && exit 1

VALGRIND_PREFIX="valgrind --show-leak-kinds=definite --leak-check=full --show-reachable=no --errors-for-leak-kinds=definite,possible --track-origins=yes --log-file=/tmp/vg.out"
for exe in ${execs[@]}
do
  echo ${VALGRIND_PREFIX} ${exe} --verbose
done

