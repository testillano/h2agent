#!/bin/bash

#############
# VARIABLES #
#############
base_ver__dflt=latest # http2comm_build
make_procs__dflt=$(grep processor /proc/cpuinfo -c)
build_type__dflt=Release
ert_http2comm_ver__dflt=v0.0.10
nlohmann_json_ver__dflt=v3.9.1
pboettch_jsonschemavalidator_ver__dflt=2.1.0

#############
# EXECUTION #
#############
cd $(dirname $0)
echo
echo "---------------------"
echo "Build 'builder image'"
echo "---------------------"
echo
echo "Input 'make_procs' [${make_procs__dflt}]:"
read make_procs
[ -z "${make_procs}" ] && make_procs=${make_procs__dflt}
bargs="--build-arg make_procs=${make_procs}"

echo "Input build type (Release|Debug) [${build_type__dflt}]:"
read build_type
[ -z "${build_type}" ] && build_type=${build_type__dflt}
bargs+=" --build-arg build_type=${build_type}"

echo "Input http2comm_build 'base_ver' [${base_ver__dflt}]:"
read base_ver
[ -z "${base_ver}" ] && base_ver=${base_ver__dflt}
bargs+=" --build-arg base_ver=${base_ver}"

echo "Input ert_http2comm version [${ert_http2comm_ver__dflt}]:"
read ert_http2comm_ver
[ -z "${ert_http2comm_ver}" ] && ert_http2comm_ver=${ert_http2comm_ver__dflt}
bargs+=" --build-arg ert_http2comm_ver=${ert_http2comm_ver}"

echo "Input nlohmann_json version [${nlohmann_json_ver__dflt}]:"
read nlohmann_json_ver
[ -z "${nlohmann_json_ver}" ] && nlohmann_json_ver=${nlohmann_json_ver__dflt}
bargs+=" --build-arg nlohmann_json_ver=${nlohmann_json_ver}"

echo "Input pboettch_jsonschemavalidator version [${pboettch_jsonschemavalidator_ver__dflt}]:"
read pboettch_jsonschemavalidator_ver
[ -z "${pboettch_jsonschemavalidator_ver}" ] && pboettch_jsonschemavalidator_ver=${pboettch_jsonschemavalidator_ver__dflt}
bargs+=" --build-arg pboettch_jsonschemavalidator_ver=${pboettch_jsonschemavalidator_ver}"

dck_dn=./docker/h2agent_build
docker build --rm ${bargs} -t testillano/h2agent_build ${dck_dn} || exit 1

echo
echo "-------------"
echo "Build project"
echo "-------------"
echo
rm -f CMakeCache.txt
envs="-e MAKE_PROCS=${make_procs} -e BUILD_TYPE=${build_type}"
docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/h2agent_build || exit 1
docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/h2agent_build "" doc || exit 1

echo
echo "-----------------------------"
echo "Build docker executable-image"
echo "-----------------------------"
echo
base_ver__dflt=latest # h2agent_build
scratch_img__dflt=alpine
scratch_img_ver__dflt=latest
bargs="--build-arg make_procs=${make_procs} --build-arg build_type=${build_type}"

echo "Input h2agent_build 'base_ver' [${base_ver__dflt}]:"
read base_ver
[ -z "${base_ver}" ] && base_ver=${base_ver__dflt}
bargs+=" --build-arg base_ver=${base_ver}"

echo "Input scratch image base [${scratch_img__dflt}]:"
read scratch_img
[ -z "${scratch_img}" ] && scratch_img=${scratch_img__dflt}
bargs+=" --build-arg scratch_img=${scratch_img}"

echo "Input scratch image base version [${scratch_img_ver__dflt}]:"
read scratch_img_ver
[ -z "${scratch_img_ver}" ] && scratch_img_ver=${scratch_img_ver__dflt}
bargs+=" --build-arg scratch_img_ver=${scratch_img_ver}"

docker build --rm ${bargs} -t testillano/h2agent .

