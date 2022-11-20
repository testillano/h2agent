#!/bin/bash

#############
# VARIABLES #
#############

REPO_DIR="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$REPO_DIR" ] && { echo "You must execute under a valid git repository !" ; exit 1 ; }

# Dependencies
nghttp2_ver=1.48.0
boost_ver=1.76.0 # safer to have this version (https://github.com/nghttp2/nghttp2/issues/1721).
ert_nghttp2_ver=v1.2.2 # to download nghttp2 patches (this must be aligned with previous: nghttp2 & boost)
ert_logger_ver=v1.0.10
jupp0r_prometheuscpp_ver=v0.13.0
civetweb_civetweb_ver=v1.14
ert_metrics_ver=v1.0.1
ert_multipart_ver=v1.0.1
ert_http2comm_ver=v2.0.0
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
  echo
  echo "Last step has failed (rc=$1). Some package installers"
  echo " could return non-zero code if already installed."
  echo
  echo "Press ENTER to continue, CTRL+C to abort ..."
  read -r dummy
  cd ${TMP_DIR}
}

#############
# EXECUTION #
#############

TMP_DIR=${REPO_DIR}/tmp.${RANDOM}
mkdir ${TMP_DIR} && cd ${TMP_DIR}
trap "cd ${REPO_DIR} && rm -rf ${TMP_DIR}" EXIT
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
echo "(c)ontinue or [s]kip [s]:"
read opt
[ -z "${opt}" ] && opt=s
if [ "${opt}" = "c" ]
then
  set -x && \
  wget https://github.com/Kitware/CMake/releases/download/v${cmake_ver}/cmake-${cmake_ver}.tar.gz && tar xvf cmake* && cd cmake*/ && \
  ./bootstrap && make -j${make_procs} && ${SUDO} make install && \
  cd .. && rm -rf * && \
  set +x
fi
) || failed $? && \

(
# boost
set -x && \
wget https://boostorg.jfrog.io/artifactory/main/release/${boost_ver}/source/boost_$(echo ${boost_ver} | tr '.' '_').tar.gz && tar xvf boost* && cd boost*/ && \
./bootstrap.sh && ${SUDO} ./b2 -j${make_procs} install && \
cd .. && rm -rf * && \
set +x
) || failed $? && \

(
${SUDO} apt-get install -y libssl-dev
) || failed $? && \

(
# nghttp2
set -x && \
wget https://github.com/testillano/nghttp2/archive/${ert_nghttp2_ver}.tar.gz && tar xvf ${ert_nghttp2_ver}.tar.gz && \
cp nghttp2*/deps/patches/nghttp2/${nghttp2_ver}/*.patch . && rm -rf nghttp2* && \
wget https://github.com/nghttp2/nghttp2/releases/download/v${nghttp2_ver}/nghttp2-${nghttp2_ver}.tar.bz2 && tar xvf nghttp2* && \
cd nghttp2*/ && for patch in ../*.patch; do patch -p1 < ${patch}; done && \
./configure --enable-asio-lib --disable-shared --enable-python-bindings=no && ${SUDO} make -j${make_procs} install && \
cd .. && rm -rf * && \
set +x
) || failed $? && \

(
# ert_logger
set -x && \
wget https://github.com/testillano/logger/archive/${ert_logger_ver}.tar.gz && tar xvf ${ert_logger_ver}.tar.gz && cd logger-*/ && \
cmake -DERT_LOGGER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && ${SUDO} make -j${make_procs} && ${SUDO} make install && \
cd .. && rm -rf * && \
set +x
) || failed $? && \

(
${SUDO} apt-get install -y libcurl4-openssl-dev && \
${SUDO} apt-get install -y zlib1g-dev
) || failed $? && \

(
# jupp0r_prometheuscpp
set -x && \
wget https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/${jupp0r_prometheuscpp_ver}.tar.gz && \
tar xvf ${jupp0r_prometheuscpp_ver}.tar.gz && cd prometheus-cpp*/3rdparty && \
wget https://github.com/civetweb/civetweb/archive/refs/tags/${civetweb_civetweb_ver}.tar.gz && \
tar xvf ${civetweb_civetweb_ver}.tar.gz && mv civetweb-*/* civetweb && cd .. && \
mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=${build_type} -DENABLE_TESTING=OFF .. && \
make -j${make_procs} && ${SUDO} make install && \
cd ../.. && rm -rf * && \
set +x
) || failed $? && \

(
# ert_metrics
set -x && \
wget https://github.com/testillano/metrics/archive/${ert_metrics_ver}.tar.gz && tar xvf ${ert_metrics_ver}.tar.gz && cd metrics-*/ && \
cmake -DERT_METRICS_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && ${SUDO} make install && \
cd .. && rm -rf * && \
set +x
) || failed $? && \

(
# ert_multipart
set -x && \
wget https://github.com/testillano/multipart/archive/${ert_multipart_ver}.tar.gz && tar xvf ${ert_multipart_ver}.tar.gz && cd multipart-*/ && \
cmake -DERT_MULTIPART_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && ${SUDO} make install && \
cd .. && rm -rf * && \
set +x
) || failed $? && \

(
# ert_http2comm
set -x && \
wget https://github.com/testillano/http2comm/archive/${ert_http2comm_ver}.tar.gz && tar xvf ${ert_http2comm_ver}.tar.gz && cd http2comm-*/ && \
cmake -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && ${SUDO} make install && \
cd .. && rm -rf * && \
set +x
) || failed $? && \

(
# nlohmann json
set -x && \
wget https://github.com/nlohmann/json/releases/download/${nlohmann_json_ver}/json.hpp && \
${SUDO} mkdir /usr/local/include/nlohmann && ${SUDO} mv json.hpp /usr/local/include/nlohmann && \
set +x
) || failed $? && \

(
# pboettch json-schema-validator
set -x && \
wget https://github.com/pboettch/json-schema-validator/archive/${pboettch_jsonschemavalidator_ver}.tar.gz && \
tar xvf ${pboettch_jsonschemavalidator_ver}.tar.gz && cd json-schema-validator*/ && mkdir build && cd build && \
cmake .. && make -j${make_procs} && ${SUDO} make install && \
cd ../.. && rm -rf * && \
set +x
) || failed $? && \

(
# google test framework
set -x && \
wget https://github.com/google/googletest/archive/refs/tags/release-${google_test_ver:1}.tar.gz && \
tar xvf release-${google_test_ver:1}.tar.gz && cd googletest-release*/ && cmake . && ${SUDO} make -j${make_procs} install && \
cd .. && rm -rf * && \
set +x
) || failed $? && \

(
# ArashPartow exprtk
set -x && \
wget https://github.com/ArashPartow/exprtk/raw/${arashpartow_exprtk_ver}/exprtk.hpp && \
${SUDO} mkdir /usr/local/include/arashpartow && ${SUDO} mv exprtk.hpp /usr/local/include/arashpartow && \
set +x
) || failed $? && \

#(
#${SUDO} apt-get install -y doxygen graphviz
#) || failed $? && \

# h2agent project root:
cd ${REPO_DIR} && cmake -DCMAKE_BUILD_TYPE=${build_type} -DSTATIC_LINKING=TRUE . && make -j${make_procs}

