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
nlohmann_json_ver__dflt=v3.9.1
pboettch_jsonschemavalidator_ver__dflt=2.1.0
google_test_ver__dflt=v1.10.0
registry=ghcr.io/testillano

#############
# FUNCTIONS #
#############
usage() {
  cat << EOF

  Usage: $0 [--builder-image|--project|--project-image|--auto]

         --builder-image: builds base image from './Dockerfile.build'.
         --project:       builds the project process using builder image.
         --project-image: builds project image from './Dockerfile'.
         --ct-image:      builds component test image from './ct/Dockerfile'.
         --auto:          builds everything using defaults. For headless mode with no default values,
                          you may prepend or export asked/environment variables for the corresponding
                          docker procedure:

         Environment variables:

         For headless mode you may prepend or export asked/environment variables for the corresponding
         docker procedure:

         --builder-image: image_tag, base_tag (http2comm), make_procs, nlohmann_json_ver, pboettch_jsonschemavalidator_ver, google_test_ver
         --project:       make_procs, build_type, base_tag (h2agent_builder)
         --project-image: image_tag, base_tag (h2agent_builder), scratch_img, scratch_img_tag, make_procs, build_type
         --ct-image:      image_tag, base_tag (alpine)
         --auto:          any of the variables above

         Other prepend variables:

         DBUILD_XTRA_OPTS: extra options to docker build.

         Examples:

         build_type=Debug $0 --builder-image
         image_tag=test1 $0 --auto
         DBUILD_XTRA_OPTS=--no-cache $0 --auto

EOF
}

# $1: variable by reference
_read() {
  local -n varname=$1

  local default=$(eval echo \$$1__dflt)
  local s_default="<null>"
  [ -n "${default}" ] && s_default="${default}"
  echo "Input '$1' value [${s_default}]:"

  if [ -n "${varname}" ]
  then
    echo "${varname}"
  else
    read -r varname
    [ -z "${varname}" ] && varname=${default}
  fi
}

build_builder_image() {
  echo
  echo "=== Build h2agent_builder image ==="
  echo
  _read image_tag
  _read base_tag
  _read make_procs
  _read build_type
  _read nlohmann_json_ver
  _read pboettch_jsonschemavalidator_ver
  _read google_test_ver

  bargs="--build-arg base_tag=${base_tag}"
  bargs+=" --build-arg make_procs=${make_procs}"
  bargs+=" --build-arg build_type=${build_type}"
  bargs+=" --build-arg nlohmann_json_ver=${nlohmann_json_ver}"
  bargs+=" --build-arg pboettch_jsonschemavalidator_ver=${pboettch_jsonschemavalidator_ver}"
  bargs+=" --build-arg google_test_ver=${google_test_ver}"

  set -x
  rm -f CMakeCache.txt
  # shellcheck disable=SC2086
  docker build --rm ${DBUILD_XTRA_OPTS} ${bargs} -f Dockerfile.build -t ${registry}/h2agent_builder:"${image_tag}" . || return 1
  set +x
}

build_project() {
  echo
  echo "=== Format source code ==="
  echo
  sources=$(find . -name "*.hpp" -o -name "*.cpp")
  docker run -i --rm -v $PWD:/data frankwolf/astyle ${sources}

  echo
  echo "=== Build h2agent project ==="
  echo
  _read base_tag
  _read make_procs
  _read build_type

  envs="-e MAKE_PROCS=${make_procs} -e BUILD_TYPE=${build_type}"

  set -x
  rm -f CMakeCache.txt
  # shellcheck disable=SC2086
  docker run --rm -it -u "$(id -u):$(id -g)" ${envs} -v "${PWD}":/code -w /code ${registry}/h2agent_builder:"${base_tag}" || return 1
  # shellcheck disable=SC2086
  docker run --rm -it -u "$(id -u):$(id -g)" ${envs} -v "${PWD}":/code -w /code ${registry}/h2agent_builder:"${base_tag}" "" doc || return 1
  set +x
}

build_project_image() {
  echo
  echo "=== Build h2agent image ==="
  echo
  _read image_tag
  _read base_tag
  _read scratch_img
  _read scratch_img_tag
  _read make_procs
  _read build_type

  bargs="--build-arg base_tag=${base_tag}"
  bargs+=" --build-arg scratch_img=${scratch_img}"
  bargs+=" --build-arg scratch_img_tag=${scratch_img_tag}"
  bargs+=" --build-arg make_procs=${make_procs}"
  bargs+=" --build-arg build_type=${build_type}"

  set -x
  rm -f CMakeCache.txt
  # shellcheck disable=SC2086
  docker build --rm ${DBUILD_XTRA_OPTS} ${bargs} -t ${registry}/h2agent:"${image_tag}" . || return 1
  set +x
}

build_ct_image() {
  echo
  echo "=== Build component test image ==="
  echo
  _read image_tag
  _read base_tag

  bargs="--build-arg base_tag=${base_tag}"

  set -x
  # shellcheck disable=SC2086
  docker build --rm ${DBUILD_XTRA_OPTS} ${bargs} -f ct/Dockerfile -t ${registry}/ct-h2agent:"${image_tag}" ct || return 1
  set +x
}

build_auto() {
  # export defaults to automate, but allow possible environment values:
  # shellcheck disable=SC1090
  source <(grep -E '^[0a-z_]+__dflt' "$0" | sed 's/^/export /' | sed 's/__dflt//' | sed -e 's/\([0a-z_]*\)=\(.*\)/\1=\${\1:-\2}/')
  build_builder_image && build_project && build_project_image && build_ct_image
}

#############
# EXECUTION #
#############
# shellcheck disable=SC2164
cd "$(dirname "$0")"

case "$1" in
  --builder-image) build_builder_image ;;
  --project) build_project ;;
  --project-image) build_project_image ;;
  --ct-image) build_ct_image ;;
  --auto) build_auto ;;
  *) usage && exit 1 ;;
esac

exit $?

