#!/bin/bash
# =============================================================================
# h2agent build script (flat multi-stage model)
# =============================================================================
# All dependency versions are declared in Dockerfile as ARGs.
# This script reads them as defaults and exposes them as --build-arg overrides.
#
# Commands:
#   --builder:       Build deps stage only (builder image with all libraries)
#   --image:         Full build: deps + compile + runtime image (default target)
#   --ct-image:      Build component test image
#   (no args):       builds everything (--image + --ct-image).
#
# Environment variables (override defaults):
#   All ARG names from Dockerfile can be set as env vars, e.g.:
#     boost_ver=1.85.0 ./build.sh --image
#     SANITIZER=asan build_type=Debug ./build.sh --image
#
# Other variables:
#   DBUILD_XTRA_OPTS: extra docker build options (e.g., --no-cache)
#   STATIC_LINKING:   TRUE/FALSE (default: FALSE)
# =============================================================================

set -e

#############
# VARIABLES #
#############
SCR="$(readlink -f "$0")"
SCR_DIR="$(dirname "${SCR}")"
cd "${SCR_DIR}"

DOCKERFILE_BUILD=Dockerfile
registry=ghcr.io/testillano

STATIC_LINKING=${STATIC_LINKING:-FALSE}

# Parse version defaults from Dockerfile (single source of truth)
parse_arg() {
  grep "^ARG ${1}=" "${DOCKERFILE_BUILD}" | head -1 | cut -d= -f2
}

# Defaults from Dockerfile
make_procs__dflt=$(grep processor /proc/cpuinfo -c)
build_type__dflt=$(parse_arg build_type)
boost_ver__dflt=$(parse_arg boost_ver)
nghttp2_ver__dflt=$(parse_arg nghttp2_ver)
nghttp2_asio_ver__dflt=$(parse_arg nghttp2_asio_ver)
ert_logger_ver__dflt=$(parse_arg ert_logger_ver)
ert_queuedispatcher_ver__dflt=$(parse_arg ert_queuedispatcher_ver)
jupp0r_prometheuscpp_ver__dflt=$(parse_arg jupp0r_prometheuscpp_ver)
civetweb_civetweb_ver__dflt=$(parse_arg civetweb_civetweb_ver)
ert_metrics_ver__dflt=$(parse_arg ert_metrics_ver)
ert_http2comm_ver__dflt=$(parse_arg ert_http2comm_ver)
nlohmann_json_ver__dflt=$(parse_arg nlohmann_json_ver)
pboettch_jsonschemavalidator_ver__dflt=$(parse_arg pboettch_jsonschemavalidator_ver)
google_test_ver__dflt=$(parse_arg google_test_ver)
arashpartow_exprtk_ver__dflt=$(parse_arg arashpartow_exprtk_ver)
ert_multipart_ver__dflt=$(parse_arg ert_multipart_ver)

image_tag__dflt=latest

#############
# FUNCTIONS #
#############
usage() {
  cat << EOF

  Usage: $0 [--builder|--image|--ct-image]

         (no args):   builds everything (--image + --ct-image).
         --builder:   builds deps stage (builder image with all libraries).
         --image:     full build: deps + compile + runtime image.
         --ct-image:  builds component test image.

         Environment variables (override any version):

           image_tag, make_procs, build_type, boost_ver, nghttp2_ver,
           nghttp2_asio_ver, ert_logger_ver, ert_queuedispatcher_ver,
           jupp0r_prometheuscpp_ver, civetweb_civetweb_ver, ert_metrics_ver,
           ert_http2comm_ver, nlohmann_json_ver, pboettch_jsonschemavalidator_ver,
           google_test_ver, arashpartow_exprtk_ver, ert_multipart_ver

         Other variables:

           DBUILD_XTRA_OPTS: extra docker build options (e.g., --no-cache)
           STATIC_LINKING:   TRUE or FALSE (default: FALSE)
           SANITIZER:        asan, tsan, or empty (default: none)

         Examples:

           $0
           boost_ver=1.85.0 $0 --image
           SANITIZER=asan build_type=Debug $0 --image
           DBUILD_XTRA_OPTS=--no-cache $0

EOF
}

# Resolve variable: use env value if set, otherwise use __dflt
resolve() {
  local var=$1
  local val="${!var}"
  if [ -z "${val}" ]; then
    val="$(eval echo \$${var}__dflt)"
  fi
  echo "${val}"
}

build_builder() {
  echo
  echo "=== Build h2agent_builder (deps stage) ==="
  echo

  local tag=$(resolve image_tag)
  local bargs=""
  bargs+=" --build-arg make_procs=$(resolve make_procs)"
  bargs+=" --build-arg build_type=$(resolve build_type)"
  bargs+=" --build-arg boost_ver=$(resolve boost_ver)"
  bargs+=" --build-arg nghttp2_ver=$(resolve nghttp2_ver)"
  bargs+=" --build-arg nghttp2_asio_ver=$(resolve nghttp2_asio_ver)"
  bargs+=" --build-arg ert_logger_ver=$(resolve ert_logger_ver)"
  bargs+=" --build-arg ert_queuedispatcher_ver=$(resolve ert_queuedispatcher_ver)"
  bargs+=" --build-arg jupp0r_prometheuscpp_ver=$(resolve jupp0r_prometheuscpp_ver)"
  bargs+=" --build-arg civetweb_civetweb_ver=$(resolve civetweb_civetweb_ver)"
  bargs+=" --build-arg ert_metrics_ver=$(resolve ert_metrics_ver)"
  bargs+=" --build-arg ert_http2comm_ver=$(resolve ert_http2comm_ver)"
  bargs+=" --build-arg nlohmann_json_ver=$(resolve nlohmann_json_ver)"
  bargs+=" --build-arg pboettch_jsonschemavalidator_ver=$(resolve pboettch_jsonschemavalidator_ver)"
  bargs+=" --build-arg google_test_ver=$(resolve google_test_ver)"
  bargs+=" --build-arg arashpartow_exprtk_ver=$(resolve arashpartow_exprtk_ver)"
  bargs+=" --build-arg ert_multipart_ver=$(resolve ert_multipart_ver)"

  set -x
  # shellcheck disable=SC2086
  docker build --rm ${DBUILD_XTRA_OPTS} ${bargs} \
    --target deps \
    -f ${DOCKERFILE_BUILD} \
    -t ${registry}/h2agent_builder:"${tag}" . || return 1
  set +x
}

build_image() {
  echo
  echo "=== Build h2agent image (full: deps + compile + runtime) ==="
  echo

  local tag=$(resolve image_tag)
  local bt=$(resolve build_type)
  local bargs=""
  bargs+=" --build-arg make_procs=$(resolve make_procs)"
  bargs+=" --build-arg build_type=${bt}"
  bargs+=" --build-arg boost_ver=$(resolve boost_ver)"
  bargs+=" --build-arg nghttp2_ver=$(resolve nghttp2_ver)"
  bargs+=" --build-arg nghttp2_asio_ver=$(resolve nghttp2_asio_ver)"
  bargs+=" --build-arg ert_logger_ver=$(resolve ert_logger_ver)"
  bargs+=" --build-arg ert_queuedispatcher_ver=$(resolve ert_queuedispatcher_ver)"
  bargs+=" --build-arg jupp0r_prometheuscpp_ver=$(resolve jupp0r_prometheuscpp_ver)"
  bargs+=" --build-arg civetweb_civetweb_ver=$(resolve civetweb_civetweb_ver)"
  bargs+=" --build-arg ert_metrics_ver=$(resolve ert_metrics_ver)"
  bargs+=" --build-arg ert_http2comm_ver=$(resolve ert_http2comm_ver)"
  bargs+=" --build-arg nlohmann_json_ver=$(resolve nlohmann_json_ver)"
  bargs+=" --build-arg pboettch_jsonschemavalidator_ver=$(resolve pboettch_jsonschemavalidator_ver)"
  bargs+=" --build-arg google_test_ver=$(resolve google_test_ver)"
  bargs+=" --build-arg arashpartow_exprtk_ver=$(resolve arashpartow_exprtk_ver)"
  bargs+=" --build-arg ert_multipart_ver=$(resolve ert_multipart_ver)"
  bargs+=" --build-arg STATIC_LINKING=${STATIC_LINKING}"

  # Sanitizer support
  case "${SANITIZER:-none}" in
    asan) bargs+=" --build-arg sanitizer_flags=-fsanitize=address"
          bargs+=" --build-arg sanitizer_extra=-fno-omit-frame-pointer"
          bargs+=" --build-arg sanitizer_link=-fsanitize=address"
          bargs+=" --build-arg sanitizer=asan"
          [ "${bt}" = "Release" ] && echo "WARNING: SANITIZER=asan works best with build_type=Debug" ;;
    tsan) bargs+=" --build-arg sanitizer_flags=-fsanitize=thread"
          bargs+=" --build-arg sanitizer_extra=-fno-omit-frame-pointer"
          bargs+=" --build-arg sanitizer_link=-fsanitize=thread"
          bargs+=" --build-arg sanitizer=tsan"
          [ "${bt}" = "Release" ] && echo "WARNING: SANITIZER=tsan works best with build_type=Debug" ;;
    none|"") ;;
    *) echo "ERROR: unknown SANITIZER '${SANITIZER}' (use: asan, tsan, none)" && return 1 ;;
  esac

  set -x
  # Build the full image using Dockerfile --target runtime (fully autonomous)
  # shellcheck disable=SC2086
  docker build --rm ${DBUILD_XTRA_OPTS} ${bargs} \
    --target runtime \
    -f ${DOCKERFILE_BUILD} \
    -t ${registry}/h2agent:"${tag}" . || return 1
  set +x

  # Also tag the builder for convenience (reuses Docker cache from above)
  echo
  echo "Tagging builder image from cache..."
  # shellcheck disable=SC2086
  docker build --rm ${bargs} \
    --target deps \
    -f ${DOCKERFILE_BUILD} \
    -t ${registry}/h2agent_builder:"${tag}" . 2>/dev/null || true
  # shellcheck disable=SC2086
  docker build --rm ${bargs} \
    --target unit-test \
    -f ${DOCKERFILE_BUILD} \
    -t ${registry}/h2agent_ut:"${tag}" . 2>/dev/null || true
}

build_ct_image() {
  echo
  echo "=== Build component test image ==="
  echo

  local tag=$(resolve image_tag)
  local bargs="--build-arg base_tag=${tag}"

  set -x
  # shellcheck disable=SC2086
  docker build --rm ${DBUILD_XTRA_OPTS} ${bargs} \
    -f ct/Dockerfile \
    -t ${registry}/ct-h2agent:"${tag}" ct || return 1
  set +x
}

build_all() {
  build_image && build_ct_image
}

#############
# EXECUTION #
#############
case "${1:-}" in
  --builder) build_builder ;;
  --image) build_image ;;
  --ct-image) build_ct_image ;;
  -h|--help) usage ;;
  "") build_all ;;
  *) usage && exit 1 ;;
esac

exit $?
