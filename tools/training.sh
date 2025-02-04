#!/bin/bash
# Kata image build helper and container execution
#NO_CACHE="--no-cache"
registry=ghcr.io/testillano

echo
git_root_dir="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$git_root_dir" ] && { echo "Go into the git repository !" ; exit 1 ; }

# Base OS:
echo "Base image (alpine/ubuntu) [ubuntu]:"
read os_type
[ -z "${os_type}" ] && os_type=ubuntu

# Build debug target:
bargs="--build-arg os_type=${os_type}"
bargs+=" --build-arg base_tag=latest"

# OpenAI Questions & Answers; only supported for ubuntu base:
qa=false
if [ "${os_type}" = "ubuntu" ]
then
  echo "Do you want to install 'OpenAI/Groq Q&A helper' dependencies (y/n) ? [n]:"
  echo " (warning: image size would be increased from 250MB to more than 8GB !)"
  read opt
  [ -z "${opt}" ] && opt=n
  [ "${opt}" = "y" ] && qa=true
fi
bargs+=" --build-arg enable_qa=${qa}"

cd ${git_root_dir}
docker build --rm ${bargs} -f Dockerfile.training  ${NO_CACHE} -t ${registry}/h2agent_training:latest . || exit 1
docker run -it --rm --entrypoint=/bin/bash -e OPENAI_API_KEY=${OPENAI_API_KEY} -e GROQ_API_KEY=${GROQ_API_KEY} ${registry}/h2agent_training:latest || exit 1
