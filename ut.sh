#!/bin/bash

# Narrow test selection by mean '--gtest_filter', i.e.:
#
# $ ./ut.sh --gtest_list_tests # to list the available tests
# $ ./ut.sh --gtest_filter=Transform_test.ProvisionWithResponseBodyAsString # to filter and run 1 specific test
# $ ./ut.sh --gtest_filter=Transform_test.* # to filter and run 1 specific suite

H2AGENT_TAG=${H2AGENT_TAG:-latest}

# Run './build.sh --auto' to have docker image available:
docker run --rm -it -v ${PWD}/build/Release/bin/unit-test:/ut --entrypoint "/ut" ghcr.io/testillano/h2agent:${H2AGENT_TAG} -- $@
