#!/bin/sh

cd /code
OUT=/code/coverage
mkdir -p ${OUT}
OPTS="--ignore-errors empty --ignore-errors negative --ignore-errors inconsistent --ignore-errors mismatch"

# Capture initial baseline (all instrumented lines with 0 hits)
lcov --capture --directory ./src --initial --output-file ${OUT}/base-coverage.info ${OPTS}

trap "" SIGTERM
/var/starter.sh "$@" &
PID=$!
wait $PID

echo "h2agent exited (SIGTERM), starting coverage analysis ..."

lcov --capture --directory ./src --output-file ${OUT}/test-coverage.info --ignore-errors gcov ${OPTS}
lcov -a ${OUT}/base-coverage.info -a ${OUT}/test-coverage.info -o ${OUT}/total-coverage.info ${OPTS}
lcov --extract ${OUT}/total-coverage.info "/code/src/*" --output-file ${OUT}/lcov.info ${OPTS}
lcov --remove ${OUT}/lcov.info "main.cpp" --output-file ${OUT}/lcov.info ${OPTS}

genhtml ${OUT}/lcov.info --output-directory ${OUT} --ignore-errors negative

touch /tmp/exit_flag
echo "Retrieve coverage artifacts and then remove '/tmp/exit_flag' to restart pod ..."
while [ -f /tmp/exit_flag ]; do sleep 1; done
echo "Exiting ..."
