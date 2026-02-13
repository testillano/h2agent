ARG base_tag=latest
ARG scratch_img=ubuntu
ARG scratch_img_tag=latest
FROM ghcr.io/testillano/h2agent_builder:${base_tag} as builder
LABEL maintainer="testillano"

LABEL testillano.h2agent.description="Docker image for h2agent service"

COPY . /code
WORKDIR /code

ARG make_procs=4
ARG build_type=Release
ARG STATIC_LINKING=FALSE

# We could duplicate from local build directory, but prefer to build from scratch:
RUN cmake -DCMAKE_BUILD_TYPE=${build_type} -DSTATIC_LINKING=${STATIC_LINKING} . && make -j${make_procs}

FROM ${scratch_img}:${scratch_img_tag}
ARG build_type=Release
COPY --from=builder /code/build/${build_type}/bin/h2agent /opt/
COPY --from=builder /code/build/${build_type}/bin/h2client /opt/
COPY --from=builder /code/build/${build_type}/bin/matching-helper /opt/
COPY --from=builder /code/build/${build_type}/bin/arashpartow-helper /opt/
COPY --from=builder /code/build/${build_type}/bin/udp-server /opt/
COPY --from=builder /code/build/${build_type}/bin/udp-server-h2client /opt/
COPY --from=builder /code/build/${build_type}/bin/udp-client /opt/

# We add curl & jq for helpers.bash
# Ubuntu has bash already installed, but vim is missing
ARG os_type=ubuntu
RUN if [ "${os_type}" = "alpine" ] ; then apk update && apk add bash curl jq nghttp2 netcat-openbsd socat libjemalloc2 && rm -rf /var/cache/apk/* ; elif [ "${os_type}" = "ubuntu" ] ; then apt-get update && apt-get install -y vim curl jq nghttp2 netcat-openbsd socat libjemalloc2 && apt-get clean ; fi

# Start script:
COPY deps/starter.sh /var

ENTRYPOINT ["sh", "/var/starter.sh" ]

CMD []
