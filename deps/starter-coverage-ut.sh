#!/bin/sh
cd /code
OUT=/code/coverage
OPTS="--ignore-errors empty --ignore-errors negative --ignore-errors inconsistent --ignore-errors inconsistent,inconsistent --ignore-errors mismatch --ignore-errors mismatch,mismatch"

lcov --capture --directory ./src --directory ./ut --initial --output-file ${OUT}/base-coverage.info ${OPTS}

/code/build/Debug/bin/unit-test

lcov --capture --directory ./src --directory ./ut --output-file ${OUT}/test-coverage.info --ignore-errors gcov,gcov ${OPTS}
lcov -a ${OUT}/base-coverage.info -a ${OUT}/test-coverage.info -o ${OUT}/total-coverage.info ${OPTS}
lcov --extract ${OUT}/total-coverage.info "/code/src/*" --output-file ${OUT}/extracted-coverage.info ${OPTS}
lcov --remove ${OUT}/extracted-coverage.info "main.cpp" --output-file ${OUT}/lcov.info ${OPTS}

# Exclude files that require integration tests (CT covers them)
lcov --remove ${OUT}/lcov.info '*/AdminClientProvision*' '*/MyAdminHttp2Server*' '*/MyTrafficHttp2Server*' --output-file ${OUT}/lcov.info ${OPTS}

genhtml ${OUT}/lcov.info --output-directory ${OUT} --ignore-errors negative
