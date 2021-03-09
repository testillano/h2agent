FROM testillano/h2agent_build:latest as builder

COPY . /code
WORKDIR /code

ARG make_procs=4
ARG build_type=Release
ARG logger_ver=v1.0.5

RUN cmake -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs}

FROM alpine:latest
ARG build_type=Release
COPY --from=builder /code/build/${build_type}/bin/h2agent /opt/h2agent

ENTRYPOINT ["/opt/h2agent"]
CMD []
