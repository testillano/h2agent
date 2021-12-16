#!/bin/bash
# Kata image build helper and container execution

echo
git_root_dir="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$git_root_dir" ] && { echo "Go into the git repository !" ; exit 1 ; }

# Build debug target:
bargs="--build-arg img_tag=latest"

cd ${git_root_dir}
docker build --rm ${bargs} -f Dockerfile.training -t testillano/h2agent_training:latest . || exit 1
docker run -it --rm --entrypoint=/bin/bash testillano/h2agent_training:latest || exit 1
