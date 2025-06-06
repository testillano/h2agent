#!/bin/bash
# XTRA_ARGS to docker (i.e. mount options) could be provided as prepend variable

H2A_TAG=${H2A_TAG:-latest}
RUN_AS="-u $(id -u)"
img_variant=
[ -n "${HTTP1_ENABLED}" ] && img_variant=_http1 && RUN_AS= # to allow creation of nghttpx configuration

# Network (i.e.: '--network=back_tier -p 8000:8000 -p 8074:8074')
NETWORK=${NETWORK:-"--network=host"}

# Recommended for 'benchmark/start.sh' (comment to use only provided arguments):
BENCH=(--verbose --traffic-server-worker-threads 5 --prometheus-response-delay-seconds-histogram-boundaries "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3")

## Rename to 'h2agent-<unique name>':
#NAME=
#(
#sleep 5
#name=$(docker ps --filter ancestor=ghcr.io/testillano/h2agent --format "{{.Names}}")
#docker rename ${name} h2agent.${name}
#) &
NAME="--name h2agent"

# Run './build.sh --auto' to have docker image available:
docker run ${RUN_AS} --rm -it ${NAME} ${NETWORK} ${XTRA_ARGS} ghcr.io/testillano/h2agent${img_variant}:${H2A_TAG} ${BENCH[*]} $@

# Using ctr-tools:
#
# Import images from docker if docker is not available:
# sudo ctr -n test images import image.tar # load image from 'docker save'
# sudo ctr -n test images list # check if correctly loaded
#
# Run image:
# sudo ctr -n test run --rm --tty --net-host ghcr.io/testillano/h2agent${img_variant}:${H2A_TAG} myapp
