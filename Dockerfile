ARG base_ver=latest
ARG scratch_img=alpine
ARG scratch_img_ver=latest
FROM testillano/h2agent_build:${base_ver} as builder

COPY . /code
WORKDIR /code

ARG make_procs=4
ARG build_type=Release

# We could duplicate from local build directory, but prefer to build from scratch:
RUN cmake -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs}

FROM ${scratch_img}:${scratch_img_ver}
ARG build_type=Release
COPY --from=builder /code/build/${build_type}/bin/h2agent /opt/h2agent

ENTRYPOINT ["/opt/h2agent"]
CMD []
