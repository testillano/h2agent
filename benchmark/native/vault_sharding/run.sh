#!/bin/bash
# =============================================================================
# Vault sharding micro-benchmark runner
# =============================================================================
# Compiles and runs benchmark/native/vault_sharding/vault_sharding_bench.cpp
# inside the h2agent builder image, where the required headers (nlohmann/json,
# ert/metrics, and the project's src/model) are available under /usr/local and
# the repo mount. This isolates the vault lock-contention effect (1 shard vs N
# shards) without any network or full-build dependency.
#
#   ./run.sh                                  # defaults
#   THREADS=16 KEYS=32 ROUNDS=20000 ./run.sh  # tune the workload
#   SWEEP=1 ./run.sh                          # matrix of THREADS x KEYS + speedup table
#
# Env knobs forwarded to the benchmark: THREADS, KEYS, ROUNDS, REPEATS, SWEEP (see the .cpp).
# Override the builder image with BUILDER_IMAGE=<image>.
#
# NOTE: this is a PERFORMANCE probe only. To check the sharded Map for data
# races, do NOT rely on this benchmark -- it exercises the Map in isolation, not
# the real Vault usage (client+server roles, transformations, managers). Use
# h2agent's native ThreadSanitizer support on the actual code instead:
#     SANITIZER=tsan build_type=Debug ./build.sh --image   # then run under load
# (or the unit-test/CT target), which instruments the real h2agent.
# =============================================================================
set -e

SCR_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
REPO_ROOT="$(cd "${SCR_DIR}/../../.." && pwd)"
BUILDER_IMAGE="${BUILDER_IMAGE:-ghcr.io/testillano/h2agent_builder:latest}"

if ! docker image inspect "${BUILDER_IMAGE}" >/dev/null 2>&1; then
  echo "Builder image '${BUILDER_IMAGE}' not found. Build it first: ./build.sh --builder" >&2
  exit 1
fi

echo "Compiling + running vault sharding micro-benchmark in ${BUILDER_IMAGE} ..."
docker run --rm -i \
  -e THREADS -e KEYS -e ROUNDS -e SWEEP -e REPEATS \
  -v "${REPO_ROOT}":/code -w /code \
  --entrypoint /bin/bash \
  "${BUILDER_IMAGE}" -c '
    set -e
    g++ -O2 -std=c++17 -pthread \
        -I src/model -I /usr/local/include \
        benchmark/native/vault_sharding/vault_sharding_bench.cpp \
        -o /tmp/vault_bench
    /tmp/vault_bench
  '
