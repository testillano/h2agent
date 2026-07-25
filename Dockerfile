# =============================================================================
# h2agent multi-stage Dockerfile
# =============================================================================
# Single file replacing the inherited chain: nghttp2 -> http2comm -> h2agent
# All dependency versions are declared as ARGs here (single source of truth).
#
# Stages:
#   deps    - All third-party libraries compiled and installed
#   build   - Project compilation (uses deps stage)
#   runtime - Minimal production image with only binaries
#
# Usage:
#   docker build --target deps    -t h2agent_builder .
#   docker build --target build   -t h2agent_build .
#   docker build --target runtime -t h2agent .
#
# The build.sh script handles all of this automatically.
# =============================================================================

FROM ubuntu:24.04 AS deps
LABEL maintainer="testillano"
LABEL testillano.h2agent_builder.description="Docker image with all dependencies to build h2agent"

WORKDIR /code/build

# ---------------------------------------------------------------------------
# Dependency versions (single source of truth)
# Order of installation is determined by the RUN steps below.
# ---------------------------------------------------------------------------
ARG make_procs=4
ARG build_type=Release

ARG boost_ver=1.84.0
ARG nghttp2_ver=1.64.0
ARG nghttp2_asio_ver=main
ARG ert_nghttp2_ver=v1.2.9
ARG ert_logger_ver=v1.1.1
ARG ert_queuedispatcher_ver=v1.0.4
ARG jupp0r_prometheuscpp_ver=v1.3.0
ARG civetweb_civetweb_ver=v1.16
ARG ert_metrics_ver=v1.2.0
ARG ert_http2comm_ver=v2.3.0
ARG nlohmann_json_ver=v3.12.0
ARG pboettch_jsonschemavalidator_ver=2.4.0
ARG google_test_ver=v1.11.0
ARG arashpartow_exprtk_ver=0.0.3
ARG ert_multipart_ver=v1.0.3

# ---------------------------------------------------------------------------
# System packages (union of nghttp2 + http2comm + h2agent requirements)
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y \
    wget zip tar bzip2 patch \
    make cmake g++ \
    libtool pkg-config autoconf automake \
    libssl-dev zlib1g-dev libcurl4-openssl-dev \
    doxygen graphviz \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# Optimization flags (LTO + portable SIMD: x86-64-v3 = AVX2, ~2013+ CPUs)
# ---------------------------------------------------------------------------
ARG OPT_CFLAGS="-O3 -march=x86-64-v3 -flto=auto"
ARG OPT_CXXFLAGS="-O3 -march=x86-64-v3 -flto=auto"
ARG OPT_LDFLAGS="-flto=auto"
ENV CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}"

# ---------------------------------------------------------------------------
# Patches (downloaded from testillano/nghttp2 repo -- single source of truth)
# ---------------------------------------------------------------------------
RUN set -x && \
    wget https://github.com/testillano/nghttp2/archive/${ert_nghttp2_ver}.tar.gz && \
    tar xf ${ert_nghttp2_ver}.tar.gz && \
    mv nghttp2-*/deps/patches /patches && \
    rm -rf nghttp2-* ${ert_nghttp2_ver}.tar.gz && \
    set +x

# ===========================================================================
# BOOST
# ===========================================================================
RUN set -x && \
    boost_tar=boost_$(echo ${boost_ver} | tr '.' '_').tar.gz && \
    wget -O ${boost_tar} https://boostorg.jfrog.io/artifactory/main/release/${boost_ver}/source/${boost_tar} && \
    file ${boost_tar} | grep -q gzip || \
    (rm -f ${boost_tar} && wget -O ${boost_tar} https://sourceforge.net/projects/boost/files/boost/${boost_ver}/${boost_tar}) && \
    tar xvf ${boost_tar} && cd boost*/ && \
    ./bootstrap.sh && ./b2 -j${make_procs} variant=release cxxflags="${OPT_CXXFLAGS}" linkflags="${OPT_LDFLAGS}" install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# NGHTTP2 (tatsuhiro library)
# ===========================================================================
RUN set -x && \
    wget https://github.com/nghttp2/nghttp2/releases/download/v${nghttp2_ver}/nghttp2-${nghttp2_ver}.tar.bz2 && \
    tar xf nghttp2-${nghttp2_ver}.tar.bz2 && cd nghttp2-${nghttp2_ver}/ && \
    for patch in $(ls /patches/nghttp2/${nghttp2_ver}/*.patch 2>/dev/null); do patch -p1 < ${patch}; done && \
    CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}" \
    ./configure --disable-shared --enable-python-bindings=no && make -j${make_procs} install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# NGHTTP2-ASIO
# ===========================================================================
RUN set -x && \
    wget https://github.com/nghttp2/nghttp2-asio/archive/refs/heads/${nghttp2_asio_ver}.zip && \
    unzip ${nghttp2_asio_ver}.zip && cd nghttp2-asio-${nghttp2_asio_ver} && \
    for patch in $(ls /patches/nghttp2-asio/${nghttp2_asio_ver}/*.patch 2>/dev/null); do patch -p1 < ${patch}; done && \
    autoreconf -i && automake && autoconf && \
    CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}" \
    ./configure --enable-shared=false && make -j${make_procs} install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# ERT_LOGGER
# ===========================================================================
RUN set -x && \
    wget https://github.com/testillano/logger/archive/${ert_logger_ver}.tar.gz && \
    tar xvf ${ert_logger_ver}.tar.gz && cd logger-*/ && \
    cmake -DERT_LOGGER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# ERT_QUEUEDISPATCHER
# ===========================================================================
RUN set -x && \
    wget https://github.com/testillano/queuedispatcher/archive/${ert_queuedispatcher_ver}.tar.gz && \
    tar xvf ${ert_queuedispatcher_ver}.tar.gz && cd queuedispatcher-*/ && \
    cmake -DERT_QUEUEDISPATCHER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# PROMETHEUS-CPP + CIVETWEB
# ===========================================================================
RUN set -x && \
    wget https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/${jupp0r_prometheuscpp_ver}.tar.gz && \
    tar xvf ${jupp0r_prometheuscpp_ver}.tar.gz && cd prometheus-cpp*/3rdparty && \
    wget https://github.com/civetweb/civetweb/archive/refs/tags/${civetweb_civetweb_ver}.tar.gz && \
    tar xvf ${civetweb_civetweb_ver}.tar.gz && mv civetweb-*/* civetweb && cd .. && \
    mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=${build_type} -DENABLE_TESTING=OFF -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON .. && \
    make -j${make_procs} && make install && \
    cd ../.. && rm -rf * && \
    set +x

# ===========================================================================
# ERT_METRICS
# ===========================================================================
RUN set -x && \
    wget https://github.com/testillano/metrics/archive/${ert_metrics_ver}.tar.gz && \
    tar xvf ${ert_metrics_ver}.tar.gz && cd metrics-*/ && \
    cmake -DERT_METRICS_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# ERT_HTTP2COMM
# ===========================================================================
RUN set -x && \
    wget https://github.com/testillano/http2comm/archive/${ert_http2comm_ver}.tar.gz && \
    tar xvf ${ert_http2comm_ver}.tar.gz && cd http2comm-*/ && \
    cmake -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# NLOHMANN JSON
# ===========================================================================
RUN set -x && \
    wget https://github.com/nlohmann/json/archive/refs/tags/${nlohmann_json_ver}.tar.gz && \
    tar xvf ${nlohmann_json_ver}.tar.gz && cd json-*/ && mkdir build && cd build && \
    cmake -DJSON_BuildTests=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && \
    make -j${make_procs} install && \
    cd ../.. && rm -rf * && \
    set +x

# ===========================================================================
# PBOETTCH JSON-SCHEMA-VALIDATOR
# ===========================================================================
RUN set -x && \
    wget https://github.com/pboettch/json-schema-validator/archive/${pboettch_jsonschemavalidator_ver}.tar.gz && \
    tar xvf ${pboettch_jsonschemavalidator_ver}.tar.gz && cd json-schema-validator*/ && mkdir build && cd build && \
    cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && \
    make -j${make_procs} && make install && \
    cd ../.. && rm -rf * && \
    set +x

# ===========================================================================
# GOOGLE TEST FRAMEWORK
# ===========================================================================
RUN set -x && \
    wget https://github.com/google/googletest/archive/refs/tags/release-$(echo ${google_test_ver} | cut -c2-).tar.gz && \
    tar xvf release-$(echo ${google_test_ver} | cut -c2-).tar.gz && cd googletest-release*/ && \
    cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 . && make -j${make_procs} install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# ARASHPARTOW EXPRTK (header-only)
# ===========================================================================
RUN set -x && \
    wget https://github.com/ArashPartow/exprtk/raw/${arashpartow_exprtk_ver}/exprtk.hpp && \
    mkdir -p /usr/local/include/arashpartow && mv exprtk.hpp /usr/local/include/arashpartow && \
    set +x

# ===========================================================================
# ERT_MULTIPART
# ===========================================================================
RUN set -x && \
    wget https://github.com/testillano/multipart/archive/${ert_multipart_ver}.tar.gz && \
    tar xvf ${ert_multipart_ver}.tar.gz && cd multipart-*/ && \
    cmake -DERT_MULTIPART_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# ---------------------------------------------------------------------------
# Builder entrypoint (cmake + make wrapper, compatible with existing --project)
# ---------------------------------------------------------------------------
COPY deps/build.sh /var/build.sh
RUN chmod a+x /var/build.sh

ENTRYPOINT ["/var/build.sh"]
CMD []

# =============================================================================
# Stage: build (compile h2agent project)
# =============================================================================
FROM deps AS build

ARG make_procs=4
ARG build_type=Release
ARG STATIC_LINKING=FALSE
ARG H2COMM_MAX_CONCURRENT_STREAMS=ON
ARG sanitizer_flags=""
ARG sanitizer_extra=""
ARG sanitizer_link=""

COPY . /code
WORKDIR /code

RUN cmake -DCMAKE_BUILD_TYPE=${build_type} -DSTATIC_LINKING=${STATIC_LINKING} \
    -DH2COMM_MAX_CONCURRENT_STREAMS=${H2COMM_MAX_CONCURRENT_STREAMS} \
    ${sanitizer_flags:+-DCMAKE_CXX_FLAGS="${sanitizer_flags} ${sanitizer_extra}"} \
    ${sanitizer_link:+-DCMAKE_EXE_LINKER_FLAGS="${sanitizer_link}"} \
    . && make -j${make_procs}

# =============================================================================
# Stage: unit-test (lightweight image for running tests)
# =============================================================================
FROM ubuntu:24.04 AS unit-test

ARG build_type=Release

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3t64 \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=build /code/build/${build_type}/bin/unit-test /opt/unit-test

ENTRYPOINT ["/opt/unit-test"]
CMD []

# =============================================================================
# Stage: runtime (minimal production image)
# =============================================================================
FROM ubuntu:24.04 AS runtime

ARG build_type=Release
ARG sanitizer=""

# Runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    vim curl jq nghttp2-client netcat-openbsd socat libjemalloc2 \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Sanitizer runtime libraries (only when needed)
RUN if [ "${sanitizer}" = "asan" ] ; then apt-get update && apt-get install -y libasan8 && apt-get clean && rm -rf /var/lib/apt/lists/* ; \
    elif [ "${sanitizer}" = "tsan" ] ; then apt-get update && apt-get install -y libtsan2 && apt-get clean && rm -rf /var/lib/apt/lists/* ; fi

# Copy all project binaries
COPY --from=build /code/build/${build_type}/bin/h2agent /opt/
COPY --from=build /code/build/${build_type}/bin/h2client /opt/
COPY --from=build /code/build/${build_type}/bin/matching-helper /opt/
COPY --from=build /code/build/${build_type}/bin/arashpartow-helper /opt/
COPY --from=build /code/build/${build_type}/bin/udp-server /opt/
COPY --from=build /code/build/${build_type}/bin/udp-server-h2client /opt/
COPY --from=build /code/build/${build_type}/bin/udp-client /opt/

# Entrypoint (nghttpx proxy + jemalloc + h2agent)
COPY --from=build /code/deps/starter.sh /var/starter.sh

ENTRYPOINT ["sh", "/var/starter.sh"]
CMD []
