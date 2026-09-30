#!/bin/bash
# =============================================================================
# MockData sharding micro-benchmark runner
# =============================================================================
# Compiles and runs benchmark/native/mock_data_sharding/mock_data_sharding_bench.cpp
# inside the h2agent builder image (headers under /usr/local + the repo mount).
# Measures whether sharding helps the MockData event-store access pattern, which
# is READ-HEAVY (transformations read past events) unlike the write-per-key Vault.
#
#   ./run.sh                                        # defaults (READ_PCT=80, HOT_PCT=10)
#   THREADS=16 KEYS=32 ROUNDS=40000 ./run.sh        # tune the workload
#   READ_PCT=50 HOT_PCT=0 ./run.sh                  # write-heavier, no hot key
#   SWEEP=1 ./run.sh                                # matrix of THREADS x KEYS + speedup table
#
# Env knobs forwarded to the benchmark: THREADS, KEYS, ROUNDS, REPEATS,
# READ_PCT, HOT_PCT, SWEEP (see the .cpp).
# Override the builder image with BUILDER_IMAGE=<image>.
#
# NOTE: performance probe only. Data-race checking is out of scope; use
# h2agent's native ThreadSanitizer build (SANITIZER=tsan) on the real code.
# =============================================================================
set -e

SCR_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
REPO_ROOT="$(cd "${SCR_DIR}/../../.." && pwd)"
BUILDER_IMAGE="${BUILDER_IMAGE:-ghcr.io/testillano/h2agent_builder:latest}"

if ! docker image inspect "${BUILDER_IMAGE}" >/dev/null 2>&1; then
  echo "Builder image '${BUILDER_IMAGE}' not found. Build it first: ./build.sh --builder" >&2
  exit 1
fi

echo "Compiling + running mock_data sharding micro-benchmark in ${BUILDER_IMAGE} ..."
docker run --rm -i \
  -e THREADS -e KEYS -e ROUNDS -e SWEEP -e REPEATS -e READ_PCT -e HOT_PCT -e GLOBAL_KEYS \
  -v "${REPO_ROOT}":/code -w /code \
  --entrypoint /bin/bash \
  "${BUILDER_IMAGE}" -c '
    set -e
    g++ -O2 -std=c++17 -pthread \
        -I src/model -I /usr/local/include \
        benchmark/native/mock_data_sharding/mock_data_sharding_bench.cpp \
        -o /tmp/mock_data_bench
    /tmp/mock_data_bench
  '
