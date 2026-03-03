#!/bin/sh

# Set workdir and coverage options
cd /code
OUT=/code/coverage
mkdir -p ${OUT}
OPTS="--ignore-errors empty --ignore-errors negative --ignore-errors inconsistent --ignore-errors inconsistent,inconsistent --ignore-errors mismatch --ignore-errors mismatch,mismatch"

# Capture initial baseline (all instrumented lines with 0 hits)
lcov --capture --directory ./src --directory ./ut --initial --output-file ${OUT}/baseline.info ${OPTS}

# Run program
/code/build/Debug/bin/unit-test
echo "Unit Test finished, starting coverage analysis ..."

# Analysis and report
lcov --capture --directory ./src --directory ./ut --output-file ${OUT}/lcov.info --ignore-errors gcov,gcov ${OPTS}
lcov -a ${OUT}/baseline.info -a ${OUT}/lcov.info -o ${OUT}/lcov.info ${OPTS}
lcov --extract ${OUT}/lcov.info "/code/src/*" --output-file ${OUT}/lcov.info ${OPTS}
lcov --remove ${OUT}/lcov.info "main.cpp" --output-file ${OUT}/lcov.info ${OPTS}
lcov --remove ${OUT}/lcov.info '*/AdminClientProvision*' '*/MyAdminHttp2Server*' '*/MyTrafficHttp2Server*' --output-file ${OUT}/lcov.info ${OPTS}
genhtml ${OUT}/lcov.info --output-directory ${OUT} --ignore-errors negative
