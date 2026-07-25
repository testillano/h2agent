#!/bin/bash

# Narrow test selection by mean '--gtest_filter', i.e.:
#
# $ ./ut.sh --gtest_list_tests # to list the available tests
# $ ./ut.sh --gtest_filter=Transform_test.ProvisionWithResponseBodyAsString # to filter and run 1 specific test
# $ ./ut.sh --gtest_filter=Transform_test.* # to filter and run 1 specific suite

H2AGENT_UT_IMAGE=${H2AGENT_UT_IMAGE:-ghcr.io/testillano/h2agent_ut:latest}

# Build unit-test image if not available:
if ! docker image inspect ${H2AGENT_UT_IMAGE} &>/dev/null; then
  echo "Building unit-test image..."
  docker build --target unit-test -t ${H2AGENT_UT_IMAGE} .
fi

docker run --rm -it ${H2AGENT_UT_IMAGE} $@
