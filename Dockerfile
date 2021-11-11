ARG base_tag=latest
ARG scratch_img=alpine
ARG scratch_img_tag=latest
FROM ghcr.io/testillano/h2agent_builder:${base_tag} as builder
MAINTAINER testillano

LABEL testillano.h2agent.description="Docker image for h2agent service"

COPY . /code
WORKDIR /code

ARG make_procs=4
ARG build_type=Release

# We could duplicate from local build directory, but prefer to build from scratch:
RUN cmake -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs}

FROM ${scratch_img}:${scratch_img_tag}
ARG build_type=Release
COPY --from=builder /code/build/${build_type}/bin/h2agent /opt/h2agent

ENTRYPOINT ["/opt/h2agent"]
CMD []
