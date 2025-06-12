ARG base_tag=latest
ARG scratch_img=ubuntu
ARG scratch_img_tag=latest
FROM ghcr.io/testillano/h2agent_builder:${base_tag} as builder
MAINTAINER testillano

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

# We add curl & jq for helpers.src
# Ubuntu has bash already installed, but vim is missing
ARG os_type=ubuntu
RUN if [ "${os_type}" = "alpine" ] ; then apk update && apk add bash curl jq nghttp2 && rm -rf /var/cache/apk/* ; elif [ "${os_type}" = "ubuntu" ] ; then apt-get update && apt-get install -y vim curl jq nghttp2 && apt-get clean ; fi

RUN printf %b "#!/bin/sh\n\
if [ -n \"\${H2AGENT_ENABLE_HTTP1_PROXY}\" ]\n\
then\n\
  echo\n\
  echo \"Launching nghttpx proxy for HTTP/1 access (non-empty value for 'H2AGENT_ENABLE_HTTP1_PROXY'):\"\n\
  echo \"H2AGENT_HTTP1_PORT=\${H2AGENT_HTTP1_PORT}\"\n\
  echo \"H2AGENT_HTTP2_PORT=\${H2AGENT_HTTP2_PORT}\"\n\
  /opt/h2agent \$@ &\n\
  cat << EOF > /tmp/nghttpx.conf\n\
frontend=0.0.0.0,\${H2AGENT_HTTP1_PORT:-8001};no-tls\n\
backend=0.0.0.0,\${H2AGENT_HTTP2_PORT:-8000};;proto=h2\n\
http2-proxy=no\n\
EOF\n\
  exec nghttpx --conf=/tmp/nghttpx.conf --host-rewrite --no-server-rewrite --no-add-x-forwarded-proto # --log-level=INFO --no-via\n\
else\n\
  exec /opt/h2agent \$@\n\
fi\n\
echo \"Exiting h2agent entrypoint\"" > /var/starter.sh

RUN chmod a+x /var/starter.sh

ENTRYPOINT ["sh", "/var/starter.sh" ]

CMD []
