#!/bin/bash
# [troubleshoot] define non-empty value for 'DEBUG' variable in order to keep build native artifacts
DEBUG=true

#############
# VARIABLES #
#############

REPO_DIR="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$REPO_DIR" ] && { echo "You must execute under a valid git repository !" ; exit 1 ; }

STATIC_LINKING=${STATIC_LINKING:-FALSE} # https://stackoverflow.com/questions/57476533/why-is-statically-linking-glibc-discouraged:

# Dependencies
nghttp2_ver=1.64.0
nghttp2_asio_ver=main
boost_ver=1.84.0 # safer to have this version (https://github.com/nghttp2/nghttp2/issues/1721).
ert_nghttp2_ver=v1.2.5 # to download nghttp2 patches (this must be aligned with previous: nghttp2 & nghttp2-asio & boost)
ert_logger_ver=v1.1.0
ert_queuedispatcher_ver=v1.0.3
jupp0r_prometheuscpp_ver=v0.13.0
civetweb_civetweb_ver=v1.14
ert_metrics_ver=v1.1.0
ert_http2comm_ver=v2.1.8
nlohmann_json_ver=$(grep ^nlohmann_json_ver__dflt= ${REPO_DIR}/build.sh | cut -d= -f2)
pboettch_jsonschemavalidator_ver=$(grep ^pboettch_jsonschemavalidator_ver__dflt= ${REPO_DIR}/build.sh | cut -d= -f2)
google_test_ver=$(grep ^google_test_ver__dflt= ${REPO_DIR}/build.sh | cut -d= -f2)
arashpartow_exprtk_ver=0.0.3
ert_multipart_ver=$(grep ^ert_multipart_ver__dflt= ${REPO_DIR}/build.sh | cut -d= -f2)

# Build requirements
cmake_ver=3.23.2
build_type=${BUILD_TYPE:-Release}
make_procs=$(grep processor /proc/cpuinfo -c)

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

# Builders
# $1: what (cmake|boost|nghttp2|nghttp2_asio|ert_logger|ert_queuedispatcher|jupp0r_prometheuscpp|ert_metrics|ert_multipart|ert_http2comm|
#           nlohmann_json|pboettch_json_schema_validator|google_test_framework|arashpartow_exprtk|project)
build() {
  if [ -n "${INSTALL_PERMISSIONS}" ]
  then
    case $1 in
      cmake) ./bootstrap && make -j${make_procs} && sudo make install ;;
      boost) ./bootstrap.sh && sudo ./b2 -j${make_procs} install ;;
      nghttp2) ./configure --disable-shared --enable-python-bindings=no && sudo make -j${make_procs} install ;;
      nghttp2_asio) autoreconf -i && automake && autoconf && ./configure --enable-shared=false && sudo make -j${make_procs} install ;;
      ert_logger) ${CMAKE} -DERT_LOGGER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && sudo make -j${make_procs} && sudo make install ;;
      ert_queuedispatcher) ${CMAKE} -DERT_QUEUEDISPATCHER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && sudo make -j${make_procs} && sudo make install ;;
      jupp0r_prometheuscpp) ${CMAKE} -DCMAKE_BUILD_TYPE=${build_type} -DENABLE_TESTING=OFF .. && make -j${make_procs} && sudo make install ;;
      ert_metrics) ${CMAKE} -DERT_METRICS_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && sudo make install ;;
      ert_multipart) ${CMAKE} -DERT_MULTIPART_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && sudo make install ;;
      ert_http2comm) ${CMAKE} -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && sudo make install ;;
      nlohmann_json) sudo mkdir /usr/local/include/nlohmann && sudo mv json.hpp /usr/local/include/nlohmann ;;
      pboettch_json_schema_validator) ${CMAKE} .. && make -j${make_procs} && sudo make install ;;
      google_test_framework) ${CMAKE} . && sudo make -j${make_procs} install ;;
      arashpartow_exprtk) sudo mkdir /usr/local/include/arashpartow && sudo mv exprtk.hpp /usr/local/include/arashpartow ;;
      project) ${CMAKE} -DCMAKE_BUILD_TYPE=${build_type} -DSTATIC_LINKING=${STATIC_LINKING} . && make -j${make_procs} ;;
      *) echo "Don't know how to build '$1' !" && return 1 ;;
    esac
  else
    case $1 in
      cmake) ./bootstrap --prefix=${TMP_DIR}/local && make -j${make_procs} && make install ;;
      boost) ./bootstrap.sh --prefix=${TMP_DIR}/local && ./b2 -j${make_procs} install ;;
      nghttp2) ./configure --disable-shared --enable-python-bindings=no --prefix=${TMP_DIR}/local --with-boost-libdir=${TMP_DIR}/local/lib && make -j${make_procs} install ;;
      nghttp2_asio) autoreconf -i && automake && autoconf && ./configure --enable-shared=false && make -j${make_procs} install ;;
      ert_logger) ${CMAKE} -DERT_LOGGER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INSTALL_PREFIX=${TMP_DIR}/local . && make -j${make_procs} && make install ;;
      ert_queuedispatcher) ${CMAKE} -DERT_QUEUEDISPATCHER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INSTALL_PREFIX=${TMP_DIR}/local . && make -j${make_procs} && make install ;;
      jupp0r_prometheuscpp) ${CMAKE} -DCMAKE_BUILD_TYPE=${build_type} -DENABLE_TESTING=OFF -DCMAKE_INSTALL_PREFIX=${TMP_DIR}/local .. && make -j${make_procs} && make install ;;
      ert_metrics) ${CMAKE} -DERT_METRICS_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INSTALL_PREFIX=${TMP_DIR}/local -DCMAKE_CXX_FLAGS=-isystem\ ${TMP_DIR}/local/include . && make -j${make_procs} && make install ;;
      ert_multipart) ${CMAKE} -DERT_MULTIPART_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INSTALL_PREFIX=${TMP_DIR}/local -DCMAKE_CXX_FLAGS=-isystem\ ${TMP_DIR}/local/include . && make -j${make_procs} && make install ;;
      ert_http2comm) ${CMAKE} -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INSTALL_PREFIX=${TMP_DIR}/local -DCMAKE_CXX_FLAGS=-isystem\ ${TMP_DIR}/local/include . && make -j${make_procs} && make install ;;
      nlohmann_json) mkdir ${TMP_DIR}/local/include/nlohmann && mv json.hpp ${TMP_DIR}/local/include/nlohmann ;;
      pboettch_json_schema_validator) ${CMAKE} -DCMAKE_INSTALL_PREFIX=${TMP_DIR}/local .. && make -j${make_procs} && make install ;;
      google_test_framework) ${CMAKE} -DCMAKE_INSTALL_PREFIX=${TMP_DIR}/local . && make -j${make_procs} install ;;
      arashpartow_exprtk) mkdir ${TMP_DIR}/local/include/arashpartow && mv exprtk.hpp ${TMP_DIR}/local/include/arashpartow ;;
      project) ${CMAKE} -DCMAKE_BUILD_TYPE=${build_type} -DSTATIC_LINKING=${STATIC_LINKING} -DCMAKE_CXX_FLAGS=-isystem\ ${TMP_DIR}/local/include -DCMAKE_PREFIX_PATH=${TMP_DIR}/local . && make ;; # TODO: does not work in parallel (so we remove: -j${make_procs})
      *) echo "Don't know how to build '$1' !" && return 1 ;;
    esac
  fi
}

#############
# EXECUTION #
#############

TMP_DIR=${REPO_DIR}/$(basename $0 .sh)
if [ -d "${TMP_DIR}" ]
then
  echo "Temporary already exists. Remove ? (y/n) [y]:"
  read opt
  [ -z "${opt}" ] && opt=y
  [ "${opt}" = "y" ] && rm -rf ${TMP_DIR}
fi

mkdir -p ${TMP_DIR} && cd ${TMP_DIR}
echo
echo "Working on temporary directory '${TMP_DIR}' ..."
echo

INSTALL_PERMISSIONS=
CMAKE=${TMP_DIR}/local/bin/cmake
echo
echo "Do you have 'make install' permissions ? (y/n) [y]:"
read opt
[ -z "${opt}" ] && opt=y
[ "${opt}" = "y" ] && INSTALL_PERMISSIONS=true && CMAKE=cmake

# Update apt
sudo apt-get update

(
echo
echo "CMake"
echo "-----"
echo "Required: cmake version 3.14"
echo "Current:  $(${CMAKE} --version 2>/dev/null | grep version)"
echo "Install:  cmake version ${cmake_ver}"
echo
ask cmake && set -x && \
  wget https://github.com/Kitware/CMake/releases/download/v${cmake_ver}/cmake-${cmake_ver}.tar.gz && tar xvf cmake* && cd cmake*/ && \
  build cmake && \
  cd .. && clean_all && \
  set +x
) || failed $? && \

ask boost && (
set -x && \
wget https://boostorg.jfrog.io/artifactory/main/release/${boost_ver}/source/boost_$(echo ${boost_ver} | tr '.' '_').tar.gz && tar xvf boost* && cd boost*/ && \
build boost && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask libssl-dev && (
sudo apt-get install -y libssl-dev libxml2-dev

) || failed $? && \

ask nghttp2 && (
set -x && \
clean_all nghttp2 && \
wget https://github.com/nghttp2/nghttp2/releases/download/v${nghttp2_ver}/nghttp2-${nghttp2_ver}.tar.bz2 && tar xvf nghttp2-${nghttp2_ver}.tar.bz2 && \
cd nghttp2-${nghttp2_ver}/ && \
build nghttp2 && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask nghttp2_asio && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/nghttp2 ${ert_nghttp2_ver} && \
cp nghttp2*/deps/patches/nghttp2-asio/${nghttp2_asio_ver}/*.patch . 2>/dev/null && \
sudo apt-get install -y libtool && \
sudo apt-get install -y pkg-config && \
sudo apt-get install -y autoconf && \
sudo apt-get install -y automake && \
wget https://github.com/nghttp2/nghttp2-asio/archive/refs/heads/${nghttp2_asio_ver}.zip && unzip ${nghttp2_asio_ver}.zip && \
cd nghttp2-asio-${nghttp2_asio_ver}/ && for patch in $(ls ../*.patch 2>/dev/null); do patch -p1 < ${patch}; done && \
build nghttp2_asio && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask ert_logger && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/logger ${ert_logger_ver} && cd logger-*/ && \
build ert_logger && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask ert_queuedispatcher && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/queuedispatcher ${ert_queuedispatcher_ver} && cd queuedispatcher-*/ && \
build ert_queuedispatcher && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask "libcurl4-openssl-dev and zlib1g-dev" && (
sudo apt-get install -y libcurl4-openssl-dev && \
sudo apt-get install -y zlib1g-dev
) || failed $? && \

ask jupp0r_prometheuscpp && (
set -x && \
wget https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/${jupp0r_prometheuscpp_ver}.tar.gz && \
tar xvf ${jupp0r_prometheuscpp_ver}.tar.gz && cd prometheus-cpp*/3rdparty && \
wget https://github.com/civetweb/civetweb/archive/refs/tags/${civetweb_civetweb_ver}.tar.gz && \
tar xvf ${civetweb_civetweb_ver}.tar.gz && mv civetweb-*/* civetweb && cd .. && \
mkdir build && cd build && \
build jupp0r_prometheuscpp && \
cd ../.. && clean_all && \
set +x
) || failed $? && \

ask ert_metrics && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/metrics ${ert_metrics_ver} && cd metrics-*/ && \
build ert_metrics && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask ert_multipart && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/multipart ${ert_multipart_ver} && cd multipart-*/ && \
build ert_multipart && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask ert_http2comm && (
set -x && \
download_and_unpack_github_archive https://github.com/testillano/http2comm ${ert_http2comm_ver} && cd http2comm-*/ && \
build ert_http2comm && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask "nlohmann json" && (
set -x && \
wget https://github.com/nlohmann/json/releases/download/${nlohmann_json_ver}/json.hpp && \
build nlohmann_json && \
set +x
) || failed $? && \

ask "pboettch json-schema-validator" && (
set -x && \
download_and_unpack_github_archive https://github.com/pboettch/json-schema-validator ${pboettch_jsonschemavalidator_ver} && cd json-schema-validator*/ && \
mkdir build && cd build && \
build pboettch_json_schema_validator && \
cd ../.. && clean_all && \
set +x
) || failed $? && \

ask "google test framework" && (
set -x && \
wget https://github.com/google/googletest/archive/refs/tags/release-${google_test_ver:1}.tar.gz && \
tar xvf release-${google_test_ver:1}.tar.gz && cd googletest-release*/ && \
build google_test_framework && \
cd .. && clean_all && \
set +x
) || failed $? && \

ask "ArashPartow exprtk" &&
(
set -x && \
wget https://github.com/ArashPartow/exprtk/raw/${arashpartow_exprtk_ver}/exprtk.hpp && \
build arashpartow_exprtk && \
set +x
) || failed $? && \

#ask "doxygen and graphviz" && (
#sudo apt-get install -y doxygen graphviz
#) || failed $? && \

# h2agent project root:
ask "main project" && cd ${REPO_DIR} && rm -rf build CMakeCache.txt CMakeFiles && build project

