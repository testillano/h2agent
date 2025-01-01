#!/bin/bash
# Coverage image build helper and report generation
registry=ghcr.io/testillano

echo
git_root_dir="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$git_root_dir" ] && { echo "Go into the git repository !" ; exit 1 ; }

# Base OS:
echo "Base image (alpine/ubuntu) [ubuntu]:"
read base_os
[ -z "${base_os}" ] && base_os=ubuntu

# Build debug target:
bargs="--build-arg base_os=${base_os}"
bargs+=" --build-arg base_tag=latest"
bargs+=" --build-arg make_procs=$(grep processor /proc/cpuinfo -c)"

cd ${git_root_dir}
rm -rf coverage
docker build --rm ${bargs} -f Dockerfile.coverage -t ${registry}/h2agent:latest-cov . || exit 1
docker run -it --rm -v ${PWD}/coverage:/code/coverage ${registry}/h2agent:latest-cov || exit 1
firefox coverage/index.html &
