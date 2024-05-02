#!/bin/bash
# Kata image build helper and container execution
#NO_CACHE="--no-cache"

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

# OpenAI Questions & Answers:
echo "Do you want to install 'OpenAI Q&A helper' dependencies (y/n) ? [n]:"
echo " (warning: image size would be increased from 250MB to more than 8GB !)"
read opt
[ -z "${opt}" ] && opt=n
qa=false
[ "${opt}" = "y" ] && qa=true
bargs+=" --build-arg enable_qa=${qa}"

cd ${git_root_dir}
docker build --rm ${bargs} -f Dockerfile.training  ${NO_CACHE} -t testillano/h2agent_training:latest . || exit 1
docker run -it --rm --entrypoint=/bin/bash -e OPENAI_API_KEY=${OPENAI_API_KEY} testillano/h2agent_training:latest || exit 1
