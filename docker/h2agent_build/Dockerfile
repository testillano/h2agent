FROM testillano/http2comm_build:latest
MAINTAINER testillano

LABEL testillano.h2agent_build.description="Docker image to build h2agent executable"

WORKDIR /code/build

ARG make_procs=4
ARG build_type=Release
ARG ert_http2comm_ver=v0.0.9
ARG nlohmann_json_ver=v3.9.1
ARG pboettch_jsonschemavalidator_ver=2.1.0

# ert_http2comm
RUN set -x && \
    wget https://github.com/testillano/http2comm/archive/${ert_http2comm_ver}.tar.gz && tar xvf ${ert_http2comm_ver}.tar.gz && cd http2comm-*/ && \
    cmake -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# nlohmann json
RUN set -x && \
    wget https://github.com/nlohmann/json/releases/download/${nlohmann_json_ver}/json.hpp && \
    mkdir /usr/local/include/nlohmann && mv json.hpp /usr/local/include/nlohmann && \
    set +x

# pboettch json-schema-validator
RUN set -x && \
    wget https://github.com/pboettch/json-schema-validator/archive/${pboettch_jsonschemavalidator_ver}.tar.gz && \
    tar xvf ${pboettch_jsonschemavalidator_ver}.tar.gz && cd json-schema-validator*/ && mkdir build && cd build && \
    cmake .. && make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

