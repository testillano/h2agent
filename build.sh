#!/bin/bash

#############
# VARIABLES #
#############
image_tag__dflt=latest
base_tag__dflt=latest
scratch_img__dflt=alpine
scratch_img_tag__dflt=latest
make_procs__dflt=$(grep processor /proc/cpuinfo -c)
build_type__dflt=Release
ert_http2comm_tag__dflt=v0.0.10
nlohmann_json_ver__dflt=v3.9.1
pboettch_jsonschemavalidator_ver__dflt=2.1.0

#############
# FUNCTIONS #
#############
usage() {
  cat << EOF

  Usage: $0 [--builder-image|--project|--project-image|--auto]

         --builder-image: builds base image from './Dockerfile.build'.
         --project:       builds the project process using builder image.
         --project-image: builds project image from './Dockerfile'.
         --auto:          builds everything using defaults. For headless mode with no default values,
                          you may prepend or export asked/environment variables for the corresponding
                          docker procedure:

         For headless mode you may prepend or export asked/environment variables for the corresponding
         docker procedure:

         --builder-image: image_tag, base_tag (http2comm), make_procs, nlohmann_json_ver, pboettch_jsonschemavalidator_ver
         --project:       make_procs, build_type
         --project-image: image_tag, base_tag (h2agent_builder), scratch_img, scratch_img_tag, make_procs, build_type
         --auto:          any of the variables above

         Example:

         build_type=Debug $0 --builder-image
         image_tag=test1 $0 --auto

EOF
}

# $1: variable by reference; $2: default value
_read() {
  local -n varname=$1
  local default=$2

  local s_default="<null>"
  [ -n "${default}" ] && s_default="${default}"
  echo "Input '$1' value [${s_default}]:"

  if [ -n "${varname}" ]
  then
    echo "${varname}"
  else
    read varname
    [ -z "${varname}" ] && varname=${default}
  fi
}

build_builder_image() {
  echo
  echo "=== Build http2comm_builder image ==="
  echo
  _read image_tag ${image_tag__dflt}
  _read base_tag ${base_tag__dflt}
  _read make_procs ${make_procs__dflt}
  _read build_type ${build_type__dflt}
  _read nlohmann_json_ver ${nlohmann_json_ver__dflt}
  _read pboettch_jsonschemavalidator_ver ${pboettch_jsonschemavalidator_ver__dflt}

  bargs="--build-arg base_tag=${base_tag}"
  bargs+=" --build-arg make_procs=${make_procs}"
  bargs+=" --build-arg build_type=${build_type}"
  bargs+=" --build-arg nlohmann_json_ver=${nlohmann_json_ver}"
  bargs+=" --build-arg pboettch_jsonschemavalidator_ver=${pboettch_jsonschemavalidator_ver}"

  set -x
  rm -f CMakeCache.txt
  docker build --rm ${bargs} -f Dockerfile.build -t testillano/h2agent_builder:${image_tag} . || return 1
  set +x
}

build_project() {
  echo
  echo "=== Build h2agent project ==="
  echo
  _read make_procs ${make_procs__dflt}
  _read build_type ${build_type__dflt}

  envs="-e MAKE_PROCS=${make_procs} -e BUILD_TYPE=${build_type}"

  set -x
  rm -f CMakeCache.txt
  docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/h2agent_builder || return 1
  docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/h2agent_builder "" doc || return 1
  set +x
}

build_project_image() {
  echo
  echo "=== Build http2comm image ==="
  echo
  _read image_tag ${image_tag__dflt}
  _read base_tag ${base_tag__dflt}
  _read scratch_img ${scratch_img__dflt}
  _read scratch_img_tag ${scratch_img_tag__dflt}
  _read make_procs ${make_procs__dflt}
  _read build_type ${build_type__dflt}

  bargs="--build-arg base_tag=${base_tag}"
  bargs+=" --build-arg scratch_img=${scratch_img}"
  bargs+=" --build-arg scratch_img_tag=${scratch_img_tag}"
  bargs+=" --build-arg make_procs=${make_procs}"
  bargs+=" --build-arg build_type=${build_type}"

  set -x
  rm -f CMakeCache.txt
  docker build --rm ${bargs} -t testillano/h2agent:${image_tag} . || return 1
  set +x
}

build_auto() {
  # export defaults to automate, but allow possible environment values:
  source <(grep -E '^[a-z_]+__dflt' $0 | sed 's/^/export /' | sed 's/__dflt//' | sed -e 's/\([a-z_]*\)=\(.*\)/\1=\${\1:-\2}/')
  build_builder_image && build_project && build_project_image
}

#############
# EXECUTION #
#############
cd $(dirname $0)

case "$1" in
  --builder-image) build_builder_image ;;
  --project) build_project ;;
  --project-image) build_project_image ;;
  --auto) build_auto ;;
  *) usage && exit 1 ;;
esac

exit $?

