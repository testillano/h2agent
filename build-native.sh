#!/bin/bash
# =============================================================================
# h2agent native build script
# =============================================================================
# Installs ALL dependencies from source and compiles the project natively.
# All versions are read from Dockerfile (single source of truth).
#
# Usage:
#   ./build-native.sh              # build everything
#   ./build-native.sh --skip boost nghttp2   # skip already-installed deps
#   ./build-native.sh --only ert_http2comm project  # rebuild specific targets
#
# Environment variables:
#   BUILD_TYPE:      Release (default) | Debug | RelWithDebInfo
#   STATIC_LINKING:  FALSE (default) | TRUE
#   PREFIX:          Install prefix (default: /usr/local)
#   MAKE_PROCS:      Parallel jobs (default: nproc)
#   Any ARG name:    Override version from Dockerfile
#                    e.g., boost_ver=1.85.0 ./build-native.sh
# =============================================================================

set -e

#############
# VARIABLES #
#############
PROJECT_ROOT="$(dirname "$(readlink -f "$0")")"
DOCKERFILE="${PROJECT_ROOT}/Dockerfile"

BUILD_TYPE=${BUILD_TYPE:-Release}
STATIC_LINKING=${STATIC_LINKING:-FALSE}
PREFIX=${PREFIX:-/usr/local}
MAKE_PROCS=${MAKE_PROCS:-$(nproc)}
TMP_DIR="${PROJECT_ROOT}/.build-native-tmp"

# Optimization flags (match Dockerfile)
OPT_CFLAGS="${OPT_CFLAGS:--O3 -march=x86-64-v3 -flto=auto}"
OPT_CXXFLAGS="${OPT_CXXFLAGS:--O3 -march=x86-64-v3 -flto=auto}"
OPT_LDFLAGS="${OPT_LDFLAGS:--flto=auto}"
export CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}"

# Parse version from Dockerfile, allow env override
ver() {
  local var=$1
  local val="${!var}"
  if [ -z "${val}" ]; then
    val=$(grep "^ARG ${var}=" "${DOCKERFILE}" | head -1 | cut -d= -f2)
  fi
  echo "${val}"
}

# Versions (all from Dockerfile, overridable via env)
boost_ver=$(ver boost_ver)
nghttp2_ver=$(ver nghttp2_ver)
nghttp2_asio_ver=$(ver nghttp2_asio_ver)
ert_nghttp2_ver=$(ver ert_nghttp2_ver)
ert_logger_ver=$(ver ert_logger_ver)
ert_queuedispatcher_ver=$(ver ert_queuedispatcher_ver)
jupp0r_prometheuscpp_ver=$(ver jupp0r_prometheuscpp_ver)
civetweb_civetweb_ver=$(ver civetweb_civetweb_ver)
ert_metrics_ver=$(ver ert_metrics_ver)
ert_http2comm_ver=$(ver ert_http2comm_ver)
nlohmann_json_ver=$(ver nlohmann_json_ver)
pboettch_jsonschemavalidator_ver=$(ver pboettch_jsonschemavalidator_ver)
google_test_ver=$(ver google_test_ver)
arashpartow_exprtk_ver=$(ver arashpartow_exprtk_ver)
ert_multipart_ver=$(ver ert_multipart_ver)

#############
# CLI ARGS  #
#############
SKIP_LIST=()
ONLY_LIST=()
mode="all"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip) shift; while [[ $# -gt 0 && ! "$1" =~ ^-- ]]; do SKIP_LIST+=("$1"); shift; done ;;
    --only) shift; mode="only"; while [[ $# -gt 0 && ! "$1" =~ ^-- ]]; do ONLY_LIST+=("$1"); shift; done ;;
    -h|--help) cat << EOF
Usage: $0 [--skip dep1 dep2 ...] [--only dep1 dep2 ...]

Dependencies (in build order):
  system_packages download_patches boost nghttp2 nghttp2_asio ert_logger
  ert_queuedispatcher prometheus_cpp ert_metrics ert_http2comm nlohmann_json
  pboettch_json_schema_validator google_test arashpartow_exprtk ert_multipart
  project

Environment variables:
  BUILD_TYPE, STATIC_LINKING, PREFIX, MAKE_PROCS, OPT_CFLAGS, OPT_CXXFLAGS, OPT_LDFLAGS
  Any version ARG name (e.g., boost_ver=1.85.0)
EOF
      exit 0 ;;
    *) echo "Unknown option: $1" && exit 1 ;;
  esac
done

#############
# FUNCTIONS #
#############
should_build() {
  local dep=$1
  if [ "${mode}" = "only" ]; then
    printf '%s\n' "${ONLY_LIST[@]}" | grep -qx "${dep}"
  else
    ! printf '%s\n' "${SKIP_LIST[@]}" | grep -qx "${dep}"
  fi
}

step() {
  local name=$1
  shift
  if should_build "${name}"; then
    echo
    echo "=== [${name}] ==="
    echo
    "$@"
  else
    echo "--- Skipping ${name}"
  fi
}

enter_tmp() {
  mkdir -p "${TMP_DIR}"
  cd "${TMP_DIR}"
}

clean_tmp() {
  cd "${TMP_DIR}" && rm -rf *
}

CMAKE="cmake"
cmake_opts="-DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON"

#############
# BUILD STEPS
#############

install_system_packages() {
  sudo apt-get update
  sudo apt-get install -y \
    wget zip tar bzip2 patch \
    make cmake g++ \
    libtool pkg-config autoconf automake \
    libssl-dev zlib1g-dev libcurl4-openssl-dev \
    doxygen graphviz
}

download_patches() {
  enter_tmp
  wget "https://github.com/testillano/nghttp2/archive/${ert_nghttp2_ver}.tar.gz"
  tar xf ${ert_nghttp2_ver}.tar.gz
  rm -rf "${PROJECT_ROOT}/.patches"
  mv nghttp2-*/deps/patches "${PROJECT_ROOT}/.patches"
  clean_tmp
}

install_boost() {
  enter_tmp
  local boost_tar=boost_$(echo ${boost_ver} | tr '.' '_').tar.gz
  wget -O ${boost_tar} "https://boostorg.jfrog.io/artifactory/main/release/${boost_ver}/source/${boost_tar}" || \
    wget -O ${boost_tar} "https://sourceforge.net/projects/boost/files/boost/${boost_ver}/${boost_tar}"
  tar xf ${boost_tar} && cd boost*/
  ./bootstrap.sh --prefix=${PREFIX}
  sudo ./b2 -j${MAKE_PROCS} variant=release cxxflags="${OPT_CXXFLAGS}" linkflags="${OPT_LDFLAGS}" install
  clean_tmp
}

install_nghttp2() {
  enter_tmp
  wget "https://github.com/nghttp2/nghttp2/releases/download/v${nghttp2_ver}/nghttp2-${nghttp2_ver}.tar.bz2"
  tar xf nghttp2-${nghttp2_ver}.tar.bz2 && cd nghttp2-${nghttp2_ver}/
  # Apply patches if any exist for this version
  for patch in $(ls ${PROJECT_ROOT}/.patches/nghttp2/${nghttp2_ver}/*.patch 2>/dev/null); do
    patch -p1 < ${patch}
  done
  CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}" \
    ./configure --disable-shared --enable-python-bindings=no --prefix=${PREFIX}
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_nghttp2_asio() {
  enter_tmp
  wget "https://github.com/nghttp2/nghttp2-asio/archive/refs/heads/${nghttp2_asio_ver}.zip"
  unzip ${nghttp2_asio_ver}.zip && cd nghttp2-asio-${nghttp2_asio_ver}
  # Apply patches
  for patch in $(ls ${PROJECT_ROOT}/.patches/nghttp2-asio/${nghttp2_asio_ver}/*.patch 2>/dev/null); do
    patch -p1 < ${patch}
  done
  autoreconf -i && automake && autoconf
  CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}" \
    ./configure --enable-shared=false --prefix=${PREFIX}
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_ert_logger() {
  enter_tmp
  wget "https://github.com/testillano/logger/archive/${ert_logger_ver}.tar.gz"
  tar xf ${ert_logger_ver}.tar.gz && cd logger-*/
  ${CMAKE} -DERT_LOGGER_BuildExamples=OFF ${cmake_opts} -DCMAKE_INSTALL_PREFIX=${PREFIX} .
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_ert_queuedispatcher() {
  enter_tmp
  wget "https://github.com/testillano/queuedispatcher/archive/${ert_queuedispatcher_ver}.tar.gz"
  tar xf ${ert_queuedispatcher_ver}.tar.gz && cd queuedispatcher-*/
  ${CMAKE} -DERT_QUEUEDISPATCHER_BuildExamples=OFF ${cmake_opts} -DCMAKE_INSTALL_PREFIX=${PREFIX} .
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_prometheus_cpp() {
  enter_tmp
  wget "https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/${jupp0r_prometheuscpp_ver}.tar.gz"
  tar xf ${jupp0r_prometheuscpp_ver}.tar.gz && cd prometheus-cpp*/3rdparty
  wget "https://github.com/civetweb/civetweb/archive/refs/tags/${civetweb_civetweb_ver}.tar.gz"
  tar xf ${civetweb_civetweb_ver}.tar.gz && mv civetweb-*/* civetweb && cd ..
  mkdir build && cd build
  ${CMAKE} ${cmake_opts} -DENABLE_TESTING=OFF -DCMAKE_INSTALL_PREFIX=${PREFIX} ..
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_ert_metrics() {
  enter_tmp
  wget "https://github.com/testillano/metrics/archive/${ert_metrics_ver}.tar.gz"
  tar xf ${ert_metrics_ver}.tar.gz && cd metrics-*/
  ${CMAKE} -DERT_METRICS_BuildExamples=OFF ${cmake_opts} -DCMAKE_INSTALL_PREFIX=${PREFIX} .
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_ert_http2comm() {
  enter_tmp
  wget "https://github.com/testillano/http2comm/archive/${ert_http2comm_ver}.tar.gz"
  tar xf ${ert_http2comm_ver}.tar.gz && cd http2comm-*/
  ${CMAKE} ${cmake_opts} -DCMAKE_INSTALL_PREFIX=${PREFIX} .
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_nlohmann_json() {
  enter_tmp
  wget "https://github.com/nlohmann/json/archive/refs/tags/${nlohmann_json_ver}.tar.gz"
  tar xf ${nlohmann_json_ver}.tar.gz && cd json-*/ && mkdir build && cd build
  ${CMAKE} -DJSON_BuildTests=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX=${PREFIX} ..
  make -j${MAKE_PROCS} install
  clean_tmp
}

install_pboettch_json_schema_validator() {
  enter_tmp
  wget "https://github.com/pboettch/json-schema-validator/archive/${pboettch_jsonschemavalidator_ver}.tar.gz"
  tar xf ${pboettch_jsonschemavalidator_ver}.tar.gz && cd json-schema-validator*/ && mkdir build && cd build
  ${CMAKE} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX=${PREFIX} ..
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_google_test() {
  enter_tmp
  wget "https://github.com/google/googletest/archive/refs/tags/release-${google_test_ver#v}.tar.gz"
  tar xf release-${google_test_ver#v}.tar.gz && cd googletest-release*/
  ${CMAKE} -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX=${PREFIX} .
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

install_arashpartow_exprtk() {
  enter_tmp
  wget "https://github.com/ArashPartow/exprtk/raw/${arashpartow_exprtk_ver}/exprtk.hpp"
  sudo mkdir -p ${PREFIX}/include/arashpartow
  sudo mv exprtk.hpp ${PREFIX}/include/arashpartow/
  clean_tmp
}

install_ert_multipart() {
  enter_tmp
  wget "https://github.com/testillano/multipart/archive/${ert_multipart_ver}.tar.gz"
  tar xf ${ert_multipart_ver}.tar.gz && cd multipart-*/
  ${CMAKE} -DERT_MULTIPART_BuildExamples=OFF ${cmake_opts} -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX=${PREFIX} .
  make -j${MAKE_PROCS} && sudo make install
  clean_tmp
}

build_project() {
  cd "${PROJECT_ROOT}"
  rm -rf build CMakeCache.txt CMakeFiles
  ${CMAKE} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DSTATIC_LINKING=${STATIC_LINKING} \
    -DCMAKE_PREFIX_PATH=${PREFIX} .
  make -j${MAKE_PROCS}
}

#############
# EXECUTION #
#############
echo "================================================================"
echo " h2agent native build"
echo "================================================================"
echo " Build type:      ${BUILD_TYPE}"
echo " Static linking:  ${STATIC_LINKING}"
echo " Install prefix:  ${PREFIX}"
echo " Parallel jobs:   ${MAKE_PROCS}"
echo " Versions source: ${DOCKERFILE}"
echo "================================================================"
echo

step system_packages    install_system_packages
step download_patches   download_patches
step boost              install_boost
step nghttp2            install_nghttp2
step nghttp2_asio       install_nghttp2_asio
step ert_logger         install_ert_logger
step ert_queuedispatcher install_ert_queuedispatcher
step prometheus_cpp     install_prometheus_cpp
step ert_metrics        install_ert_metrics
step ert_http2comm      install_ert_http2comm
step nlohmann_json      install_nlohmann_json
step pboettch_json_schema_validator install_pboettch_json_schema_validator
step google_test        install_google_test
step arashpartow_exprtk install_arashpartow_exprtk
step ert_multipart      install_ert_multipart
step project            build_project

# Cleanup temp dir
rm -rf "${TMP_DIR}" "${PROJECT_ROOT}/.patches"

echo
echo "================================================================"
echo " Build complete!"
echo " Binaries: ${PROJECT_ROOT}/build/${BUILD_TYPE}/bin/"
echo "================================================================"
