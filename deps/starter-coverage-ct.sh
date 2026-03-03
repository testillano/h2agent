#!/bin/sh

# Set workdir and coverage options
cd /code
OUT=/code/coverage
mkdir -p ${OUT}
OPTS="--ignore-errors empty --ignore-errors negative --ignore-errors inconsistent --ignore-errors mismatch"

# Capture initial baseline (all instrumented lines with 0 hits)
lcov --capture --directory ./src --initial --output-file ${OUT}/baseline.info ${OPTS}

# Run program
trap "" SIGTERM
/var/starter.sh "$@" &
PID=$!
wait $PID
echo "h2agent exited (SIGTERM), starting coverage analysis ..."

# Analysis and report
lcov --capture --directory ./src --output-file ${OUT}/lcov.info --ignore-errors gcov ${OPTS}
lcov -a ${OUT}/baseline.info -a ${OUT}/lcov.info -o ${OUT}/lcov.info ${OPTS}
lcov --extract ${OUT}/lcov.info "/code/src/*" --output-file ${OUT}/lcov.info ${OPTS}
lcov --remove ${OUT}/lcov.info "main.cpp" --output-file ${OUT}/lcov.info ${OPTS}
genhtml ${OUT}/lcov.info --output-directory ${OUT} --ignore-errors negative

# Retain until artifacts are retrieved (this runs on kubernetes pod and volume is not persistent)
touch /tmp/exit_flag
echo "Retrieve coverage artifacts and then remove '/tmp/exit_flag' to restart pod ..."
while [ -f /tmp/exit_flag ]; do sleep 1; done
echo "Exiting ..."
