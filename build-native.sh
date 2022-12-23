#!/bin/bash
# [troubleshoot] define non-empty value for 'DEBUG' variable in order to keep build native artifacts

#############
# VARIABLES #
#############

REPO_DIR="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$REPO_DIR" ] && { echo "You must execute under a valid git repository !" ; exit 1 ; }

STATIC_LINKING=${STATIC_LINKING:-FALSE} # https://stackoverflow.com/questions/57476533/why-is-statically-linking-glibc-discouraged:

# Dependencies
nghttp2_ver=1.48.0
boost_ver=1.76.0 # safer to have this version (https://github.com/nghttp2/nghttp2/issues/1721).
ert_nghttp2_ver=v1.2.3 # to download nghttp2 patches (this must be aligned with previous: nghttp2 & boost)
ert_logger_ver=v1.0.10
jupp0r_prometheuscpp_ver=v0.13.0
civetweb_civetweb_ver=v1.14
ert_metrics_ver=v1.0.1
ert_multipart_ver=v1.0.1
ert_http2comm_ver=v2.0.1
nlohmann_json_ver=$(grep ^nlohmann_json_ver__dflt= ${REPO_DIR}/build.sh | cut -d= -f2)
pboettch_jsonschemavalidator_ver=$(grep ^pboettch_jsonschemavalidator_ver__dflt= ${REPO_DIR}/build.sh | cut -d= -f2)
google_test_ver=$(grep ^google_test_ver__dflt= ${REPO_DIR}/build.sh | cut -d= -f2)
arashpartow_exprtk_ver=0.0.1

# Build requirements
cmake_ver=3.23.2
build_type=${BUILD_TYPE:-Release}
make_procs=$(grep processor /proc/cpuinfo -c)
SUDO=${SUDO:-sudo}

#############
# FUNCTIONS #
#############
failed() {
  [ -n "${DEBUG}" ] && return 0
  echo
  echo "Last step has failed (rc=$1). Some package installers"
  echo " could return non-zero code if already installed."
  echo
  echo "Press ENTER to continue, CTRL+C to abort ..."
  read -r dummy
  cd ${TMP_DIR}
}

# $1: optional prefix to clean up
clean_all() {
  [ -n "${DEBUG}" ] && return 0
  rm -rf ${1}*
}

# $1: what
ask() {
  [ -z "${DEBUG}" ] && return 0
  echo "Continue with '$1' ? (y/n) [y]:"
  read opt
  [ -z "${opt}" ] && opt=y

  [ "${opt}" = "y" ]
}

# $1: project URL; $2: tar gz version
download_and_unpack_github_archive() {
   local project=$1
   local version=$2

   local target=${project##*/}.tar.gz # URL basename
   wget $1/archive/$2.tar.gz -O ${target}  && tar xvf ${target}
}

#############
# EXECUTION #
#############

TMP_DIR=${REPO_DIR}/$(basename $0 .sh)
if [ -d "${TMP_DIR}" ]
then
  echo "Temporary already exists. Keep it ? (y/n) [y]:"
  read opt
  [ -z "${opt}" ] && opt=y
  [ "${opt}" != "y" ] && rm -rf ${TMP_DIR}
fi

mkdir -p ${TMP_DIR} && cd ${TMP_DIR}
echo
echo "Working on temporary directory '${TMP_DIR}' ..."
echo

# Update apt
${SUDO} apt-get update

(
echo
echo "CMake"
echo "-----"
echo "Required: cmake version 3.14"
echo "Current:  $(cmake --version 2>/dev/null | grep version)"
echo "Install:  cmake version ${cmake_ver}"
echo
ask cmake && set -x && \
  wget https://github.com/Kitware/CMake/releases/download/v${cmake_ver}/cmake-${cmake_ver}.tar.gz && tar xvf cmake* && cd cmake*/ && \
  ./bootstrap && make -j${make_procs} && ${SUDO} make install && \
  cd .. && clean_all && \
  set +x
) || failed $? && \

ask boost && (
set -x && \
wget https://boostorg.jfrog.io/artifactory/main/release/${boost_ver}/source/boost_$(echo ${boost_ver} | tr '.' '_').tar.gz && tar xvf boost* && cd boost*/ && \
./bootstrap.sh && ${SUDO} ./b2 -j${make_procs} install && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask libssl-dev && (
${SUDO} apt-get install -y libssl-dev
) || failed $? && \

ask nghttp2 && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/nghttp2 ${ert_nghttp2_ver} && \
cp nghttp2*/deps/patches/nghttp2/${nghttp2_ver}/*.patch . && clean_all nghttp2 && \
wget https://github.com/nghttp2/nghttp2/releases/download/v${nghttp2_ver}/nghttp2-${nghttp2_ver}.tar.bz2 && tar xvf nghttp2-${nghttp2_ver}.tar.bz2 && \
cd nghttp2-${nghttp2_ver}/ && for patch in ../*.patch; do patch -p1 < ${patch}; done && \
./configure --enable-asio-lib --disable-shared --enable-python-bindings=no && ${SUDO} make -j${make_procs} install && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask ert_logger && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/logger ${ert_logger_ver} && cd logger-*/ && \
cmake -DERT_LOGGER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && ${SUDO} make -j${make_procs} && ${SUDO} make install && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask "libcurl4-openssl-dev and zlib1g-dev" && (
${SUDO} apt-get install -y libcurl4-openssl-dev && \
${SUDO} apt-get install -y zlib1g-dev
) || failed $? && \

ask jupp0r_prometheuscpp && (
set -x && \
wget https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/${jupp0r_prometheuscpp_ver}.tar.gz && \
tar xvf ${jupp0r_prometheuscpp_ver}.tar.gz && cd prometheus-cpp*/3rdparty && \
wget https://github.com/civetweb/civetweb/archive/refs/tags/${civetweb_civetweb_ver}.tar.gz && \
tar xvf ${civetweb_civetweb_ver}.tar.gz && mv civetweb-*/* civetweb && cd .. && \
mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=${build_type} -DENABLE_TESTING=OFF .. && \
make -j${make_procs} && ${SUDO} make install && \
cd ../.. && clean_all && \
set +x
) || failed $? && \

ask ert_metrics && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/metrics ${ert_metrics_ver} && cd metrics-*/ && \
cmake -DERT_METRICS_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && ${SUDO} make install && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask ert_multipart && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/multipart ${ert_multipart_ver} && cd multipart-*/ && \
cmake -DERT_MULTIPART_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && ${SUDO} make install && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask ert_http2comm && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/http2comm ${ert_http2comm_ver} && cd http2comm-*/ && \
cmake -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && ${SUDO} make install && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask "nlohmann json" && (
set -x && \
wget https://github.com/nlohmann/json/releases/download/${nlohmann_json_ver}/json.hpp && \
${SUDO} mkdir /usr/local/include/nlohmann && ${SUDO} mv json.hpp /usr/local/include/nlohmann && \
set +x
) || failed $? && \

ask "pboettch json-schema-validator" && (
set -x && \
download_and_unpack_github_archive https://github.com/pboettch/json-schema-validator ${pboettch_jsonschemavalidator_ver} && cd json-schema-validator*/ && \
mkdir build && cd build && \
cmake .. && make -j${make_procs} && ${SUDO} make install && \
cd ../.. && clean_all && \
set +x
) || failed $? && \

ask "google test framework" && (
set -x && \
wget https://github.com/google/googletest/archive/refs/tags/release-${google_test_ver:1}.tar.gz && \
tar xvf release-${google_test_ver:1}.tar.gz && cd googletest-release*/ && cmake . && ${SUDO} make -j${make_procs} install && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask "ArashPartow exprtk" &&
(
set -x && \
wget https://github.com/ArashPartow/exprtk/raw/${arashpartow_exprtk_ver}/exprtk.hpp && \
${SUDO} mkdir /usr/local/include/arashpartow && ${SUDO} mv exprtk.hpp /usr/local/include/arashpartow && \
set +x
) || failed $? && \

#ask "doxygen and graphviz" && (
#${SUDO} apt-get install -y doxygen graphviz
#) || failed $? && \

# h2agent project root:
ask "MAIN PROJECT" && cd ${REPO_DIR} && rm -rf build CMakeCache.txt CMakeFiles && cmake -DCMAKE_BUILD_TYPE=${build_type} -DSTATIC_LINKING=${STATIC_LINKING} . && make -j${make_procs}

