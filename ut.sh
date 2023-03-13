#!/bin/bash
docker run -it --rm -v ${PWD}/build/Release/bin/unit-test:/ut --entrypoint "/ut" ghcr.io/testillano/h2agent:latest
