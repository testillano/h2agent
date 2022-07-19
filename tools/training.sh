#!/bin/bash
# Kata image build helper and container execution

echo
git_root_dir="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$git_root_dir" ] && { echo "Go into the git repository !" ; exit 1 ; }

# Base OS:
echo "Base image (alpine/ubuntu) [ubuntu]:"
read base_os
[ -z "${base_os}" ] && base_os=ubuntu

# Build debug target:
bargs="--build-arg base_os=${base_os}"
bargs+=" --build-arg img_tag=latest"

cd ${git_root_dir}
docker build --rm ${bargs} -f Dockerfile.training -t testillano/h2agent_training:latest . || exit 1
docker run -it --rm --entrypoint=/bin/bash testillano/h2agent_training:latest || exit 1
