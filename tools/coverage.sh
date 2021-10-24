#!/bin/bash
# Coverage helper

echo
git_root_dir="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$git_root_dir" ] && { echo "Go into the git repository !" ; exit 1 ; }

# Build debug target:
bargs="--build-arg base_tag=latest"
bargs+=" --build-arg scratch_img=alpine"
bargs+=" --build-arg scratch_img_tag=latest"
bargs+=" --build-arg make_procs=$(grep processor /proc/cpuinfo -c)"

cd ${git_root_dir}
rm -rf coverage
docker build --rm ${bargs} -f Dockerfile.coverage -t testillano/h2agent:latest-cov . || return 1
docker run -it --rm -v ${PWD}/coverage:/code/coverage testillano/h2agent:latest-cov || return 1
firefox coverage/index.html &
