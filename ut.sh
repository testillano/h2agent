#!/bin/bash
H2AGENT_TAG=${H2AGENT_TAG:-latest}

# Run './build.sh --auto' to have docker image available:
docker run --rm -it -v ${PWD}/build/Release/bin/unit-test:/ut --entrypoint "/ut" ghcr.io/testillano/h2agent:${H2AGENT_TAG} -- $@
