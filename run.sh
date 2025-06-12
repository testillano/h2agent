#!/bin/bash
#
# Prepend variables:
#
#   H2AGENT_DCK_TAG:             H2agent docker image tag. Defaults to 'latest'.
#   H2AGENT_DCK_NAME:            H2agent docker container name. Defaults to 'h2agent'.
#   H2AGENT_DCK_EXTRA_ARGS:      Arguments for docker run, for example mount options, network options (--network=back_tier -p 8000:8000 -p 8001:8001 -p 8074:8074), etc.
#                                Defaults to '--network=host'.
#   H2AGENT_TRAFFIC_PROXY_PORT:  Traffic proxy port provided by nghttpx allowing http1.0, http1.1, http2 and http2 without http1 upgrade.
#   H2AGENT_TRAFFIC_SERVER_PORT: Traffic server port. Defaults to 8000. Must be aligned with '--traffic-server-port' h2agent parameter (to avoid 502 Bad Gateway error).
#
H2AGENT_DCK_TAG=${H2AGENT_DCK_TAG:-latest}
H2AGENT_DCK_NAME=${H2AGENT_DCK_NAME:-h2agent}
H2AGENT_DCK_EXTRA_ARGS=${H2AGENT_DCK_EXTRA_ARGS:-"--network=host"}

docker_args="--rm -it --name ${H2AGENT_DCK_NAME} -u $(id -u)"
if [ -n "${H2AGENT_TRAFFIC_PROXY_PORT}" ]
then
  docker_args+=" -e H2AGENT_TRAFFIC_SERVER_PORT=${H2AGENT_TRAFFIC_SERVER_PORT}"
  docker_args+=" -e H2AGENT_TRAFFIC_PROXY_PORT=${H2AGENT_TRAFFIC_PROXY_PORT}"
fi

docker_args+=" ${H2AGENT_DCK_EXTRA_ARGS}"

# Recommended for 'benchmark/start.sh' (comment to use only provided arguments):
benchmark_args=(--verbose --traffic-server-worker-threads 5 --prometheus-response-delay-seconds-histogram-boundaries "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3")

# Run './build.sh --auto' to have docker image available:
docker run ${docker_args} ghcr.io/testillano/h2agent:${H2AGENT_DCK_TAG} ${benchmark_args[*]} $@

# Using ctr-tools:
#
# Import images from docker if docker is not available:
# sudo ctr -n test images import image.tar # load image from 'docker save'
# sudo ctr -n test images list # check if correctly loaded
#
# Run image:
# sudo ctr -n test run --rm --tty --net-host ghcr.io/testillano/h2agent:${H2AGENT_DCK_TAG} myapp
