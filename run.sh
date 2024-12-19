#!/bin/bash
H2A_TAG=${H2A_TAG:-latest}
img_variant=
[ -n "${HTTP1_ENABLED}" ] && img_variant=_http1

# Recommended for 'benchmark/start.sh' (comment to use only provided arguments):
BENCH=(--verbose --traffic-server-worker-threads 5 --prometheus-response-delay-seconds-histogram-boundaries "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3")

# Run './build.sh --auto' to have docker image available:
docker run -u $(id -u) --rm -it --network=host ghcr.io/testillano/h2agent${img_variant}:${H2A_TAG} ${BENCH[*]} $@

# Using ctr-tools:
#
# Import images from docker if docker is not available:
# sudo ctr -n test images import image.tar # load image from 'docker save'
# sudo ctr -n test images list # check if correctly loaded
#
# Run image:
# sudo ctr -n test run --rm --tty --net-host ghcr.io/testillano/h2agent${img_variant}:${H2A_TAG} myapp
