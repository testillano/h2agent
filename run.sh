#!/bin/bash

###############################################################################
# Run h2agent in a Docker container.
#
# Environment variables (all optional):
#
#   H2A_DOCKER_IMAGE Docker image (default: ghcr.io/testillano/h2agent:latest)
#   H2A_DOCKER_NAME  Container name (default: h2agent)
#   H2A_DOCKER_OPTS  Extra docker run args (default: --network=host)
#                    Examples:
#                      --network=back_tier -p 8000:8000 -p 8001:8001
#                      --cap-add=SYS_PTRACE
#                      --entrypoint /opt/udp-server-h2client
#                      -e LD_PRELOAD=  (disable jemalloc)
#                      -e MALLOC_CONF=dirty_decay_ms:-1,muzzy_decay_ms:-1
#                      -e H2AGENT_TRAFFIC_PROXY_PORT=8000 (nghttpx proxy)
#                      -e H2AGENT_ADMIN_PROXY_PORT=8074
#   H2A_CLI_ARGS     H2Agent CLI arguments. Defaults to sensible benchmarking
#                    config. Set to "" to pass only positional args ($@).
#
# Usage:
#   ./run.sh                                          # defaults
#   ./run.sh --traffic-client-connections 8           # append args
#   H2A_DOCKER_IMAGE=my-registry/h2agent:dev ./run.sh # custom image
#   H2A_DOCKER_OPTS="--cap-add=SYS_PTRACE" ./run.sh   # debug mode
#   H2A_CLI_ARGS="" ./run.sh --verbose -l Debug       # only your args
#
###############################################################################

[ "$1" = "--run-help" ] && { head -30 "$0" | tail -28; exit 0; }

docker_image=${H2A_DOCKER_IMAGE:-ghcr.io/testillano/h2agent:latest}
docker_name=${H2A_DOCKER_NAME:-h2agent}
docker_opts=${H2A_DOCKER_OPTS:---network=host}

default_args="--verbose --traffic-server-worker-threads $(( $(nproc) / 2 > 2 ? $(nproc) / 2 : 2 )) --prometheus-response-delay-seconds-histogram-boundaries 100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3"
h2agent_args=${H2A_CLI_ARGS-${default_args}} # defaults if H2A_CLI_ARGS undefined

set -x
docker run --rm -it --name "${docker_name}" -u "$(id -u)" ${docker_opts} "${docker_image}" ${h2agent_args} "$@"

# Without docker (ctr-tools):
#   sudo ctr -n test images import image.tar   # from 'docker save'
#   sudo ctr -n test run --rm --tty --net-host <image> myapp
