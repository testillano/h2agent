#!/bin/bash
cd $(dirname $0)
envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/h2agent_build
docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/h2agent_build "" doc
