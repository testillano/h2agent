#!/bin/bash
# A/B performance comparison of two h2agent image tags over the same benchmark
# profiles (start.sh is reused unchanged for each single run).
#
# Primary efficiency metric: CPU time consumed by the process under test per
# request (server) or per provision (client), robust to host load.
# Noise control: dedicated ports (the host may run other h2agent instances),
# fresh container per run, interleaved rounds with alternating order (cancels
# thermal/background drift), medians over rounds, optional CPU pinning.
# Correctness guard: a run whose non-2xx/failed count differs from the base is
# flagged INVALID (a faster but wrong candidate must never pass).

#############
# VARIABLES #
#############
SCR_DIR=$(readlink -f "$(dirname "$0")")
IMAGE=${IMAGE:-ghcr.io/testillano/h2agent}
BASE_TAG=orig
CAND_TAG=latest
ROUNDS=3
TESTS=()
DEFAULT_TESTS=(server/light server/default server/ipv4-with-regexreplace server/many-rules client/default)
ADMIN_PORT=${ADMIN_PORT:-18074}
TRAFFIC_PORT=${TRAFFIC_PORT:-18000}
PROM_PORT=${PROM_PORT:-18080}
SERVER_CPUS=${SERVER_CPUS:-}     # cpuset for the server under test (e.g. "2-3")
LOAD_CPUS=${LOAD_CPUS:-}         # cpuset for start.sh/h2load (e.g. "4-5")
SERVER_NAME=h2agent-bench-server
HISTOGRAM="100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3"

usage() {
  cat << EOF
Usage: $0 [--base <tag>] [--cand <tag>] [--rounds <n>] [--test <mode/name>]... [-h|--help]

  --base    Baseline image tag (default: ${BASE_TAG}).
  --cand    Candidate image tag (default: ${CAND_TAG}).
  --rounds  Interleaved rounds per profile; medians are reported (default: ${ROUNDS}).
  --test    Profile under benchmark/tests (repeatable). Default:
            ${DEFAULT_TESTS[*]}

  Prepend variables:
    IMAGE (${IMAGE}), ADMIN_PORT/TRAFFIC_PORT/PROM_PORT (${ADMIN_PORT}/${TRAFFIC_PORT}/${PROM_PORT};
    client profiles also use admin+1 and prometheus+1), SERVER_CPUS, LOAD_CPUS,
    OUT_DIR.

  Client profiles: the server fixture always runs the BASE tag, so only the
  client under test changes between runs.

  Verdict (req/s, cps, cpu per request/provision): ranges over rounds are compared,
  so host noise is not mistaken for a change. BETTER/WORSE when the candidate range
  is fully above/below the base range (in the good/bad direction), "~" when they
  overlap (no significant difference).

  Exit code: 0 no WORSE/INVALID, 2 any WORSE or INVALID, 1 setup error.

Example:
  SERVER_CPUS=2-3 LOAD_CPUS=4-5 $0 --base latest --cand refactor --rounds 5
EOF
}

die() { echo "ERROR: $*" >&2; exit 1; }

port_busy() { ss -ltn 2>/dev/null | awk '{print $4}' | grep -qE ":${1}\$"; }

stop_server() { docker rm -f ${SERVER_NAME} &>/dev/null; }

# $1: tag
start_server() {
  local tag=$1
  local cpus_opt=()
  [ -n "${SERVER_CPUS}" ] && cpus_opt=(--cpuset-cpus "${SERVER_CPUS}")
  stop_server
  docker run --rm -d --network=host --name ${SERVER_NAME} -u "$(id -u)" "${cpus_opt[@]}" \
    "${IMAGE}:${tag}" \
    --admin-port "${ADMIN_PORT}" --traffic-server-port "${TRAFFIC_PORT}" --prometheus-port "${PROM_PORT}" \
    --prometheus-response-delay-seconds-histogram-boundaries "${HISTOGRAM}" \
    --log-level Warning > /dev/null || return 1
  local i
  for i in $(seq 1 50); do
    curl -sf --http2-prior-knowledge "http://0.0.0.0:${ADMIN_PORT}/admin/v1/configuration" -o /dev/null && return 0
    sleep 0.1
  done
  return 1
}

# $1: report file, $2: row label -> prints field $3 (1-based, after the label column)
report_field() {
  awk -F'|' -v label="$2" -v col="$3" 'index($2, label) == 2 { gsub(/ /, "", $(col + 2)); print $(col + 2); exit }' "$1"
}

# Convert h2load durations (us/ms/s) to microseconds.
to_us() {
  awk -v v="$1" 'BEGIN { n = v + 0; if (v ~ /us$/) print n; else if (v ~ /ms$/) print n * 1000; else if (v ~ /s$/) print n * 1000000; else print "" }'
}

# Prometheus Avg/p50/p99 cells are like "0.288ms".
ms_to_us() { awk -v v="$1" 'BEGIN { n = v + 0; if (v ~ /ms$/) print n * 1000; else if (v ~ /us$/) print n; else print "" }'; }

# $1: test, $2: tag, $3: round -> appends metrics to RESULTS
run_one() {
  local test=$1 tag=$2 round=$3
  local mode=${test%%/*}
  local server_tag=${tag}
  [ "${mode}" = "client" ] && server_tag=${BASE_TAG}  # fixed fixture for client profiles
  local log="${OUT_DIR}/${test//\//_}_${tag}_r${round}.log"

  start_server "${server_tag}" || { echo "  server ${IMAGE}:${server_tag} did not start"; stop_server; return 1; }
  local pid
  pid=$(docker inspect -f '{{.State.Pid}}' ${SERVER_NAME})

  local taskset_cmd=()
  [ -n "${LOAD_CPUS}" ] && taskset_cmd=(taskset -c "${LOAD_CPUS}")
  H2AGENT__ADMIN_PORT=${ADMIN_PORT} H2AGENT__TRAFFIC_PORT=${TRAFFIC_PORT} H2AGENT__PROMETHEUS_PORT=${PROM_PORT} \
    H2AGENT_PID=${pid} H2A_DOCKER_IMAGE_BN=${IMAGE} H2A_DOCKER_IMAGE_TAG=${tag} \
    "${taskset_cmd[@]}" "${SCR_DIR}/start.sh" -y --test "tests/${test}" > "${log}" 2>&1
  local rc=$?
  stop_server

  local report
  report=$(sed -n 's/^Report generated: //p' "${log}" | tail -1)
  [ ${rc} -ne 0 -o ! -f "${report}" ] && { echo "  run failed (rc=${rc}), see ${log}"; return 1; }
  mv "${report}" "${OUT_DIR}/${test//\//_}_${tag}_r${round}.md"
  report="${OUT_DIR}/${test//\//_}_${tag}_r${round}.md"

  local cpu_ms units cpu_per
  cpu_ms=$(report_field "${report}" "CPU time (ms)" 1)
  if [ "${mode}" = "server" ]; then
    local rps mean bad p50 p99
    rps=$(sed -n 's/^finished in [^,]*, \([0-9.]*\) req\/s.*/\1/p' "${log}" | tail -1)
    mean=$(to_us "$(awk '/^time for request:/ { print $6; exit }' "${log}")")
    bad=$(awk '/^status codes:/ { n = $5 + $7 + $9 } /^requests:/ { f = $10 + $12 + $14 } END { print n + f }' "${log}")
    p50=$(ms_to_us "$(report_field "${report}" "server" 4)")
    p99=$(ms_to_us "$(report_field "${report}" "server" 6)")
    echo -e "${test}\t${tag}\t${round}\treq/s\t${rps}" >> "${RESULTS}"
    echo -e "${test}\t${tag}\t${round}\tclient mean (us)\t${mean}" >> "${RESULTS}"
    echo -e "${test}\t${tag}\t${round}\tserver p50 (us)\t${p50}" >> "${RESULTS}"
    echo -e "${test}\t${tag}\t${round}\tserver p99 (us)\t${p99}" >> "${RESULTS}"
    echo -e "${test}\t${tag}\t${round}\tnon-2xx+failed\t${bad}" >> "${RESULTS}"
    units=$(awk '/^requests:/ { print $2; exit }' "${log}")
    cpu_per=$(awk -v ms="${cpu_ms}" -v n="${units}" 'BEGIN { if (ms != "" && n > 0) printf "%.2f", ms * 1000 / n }')
    echo -e "${test}\t${tag}\t${round}\tcpu/request (us)\t${cpu_per}" >> "${RESULTS}"
    echo "  req/s=${rps} mean=${mean}us server p50=${p50}us p99=${p99}us cpu/req=${cpu_per}us bad=${bad}"
  else
    local cps
    cps=$(sed -n 's/^Completed: .*(~\([0-9]*\) cps).*/\1/p' "${log}" | tail -1)
    units=$(sed -n 's/^Completed: \([0-9]*\) provisions.*/\1/p' "${log}" | tail -1)
    cpu_per=$(awk -v ms="${cpu_ms}" -v n="${units}" 'BEGIN { if (ms != "" && n > 0) printf "%.2f", ms * 1000 / n }')
    echo -e "${test}\t${tag}\t${round}\tcps\t${cps}" >> "${RESULTS}"
    echo -e "${test}\t${tag}\t${round}\tcpu/provision (us)\t${cpu_per}" >> "${RESULTS}"
    echo "  cps=${cps} cpu/provision=${cpu_per}us"
  fi
}

# "min max" of the values for (test, tag, metric).
range_of() {
  awk -F'\t' -v t="$1" -v g="$2" -v m="$3" '$1 == t && $2 == g && $4 == m && $5 != "" {
    v = $5 + 0; if (n++ == 0 || v < lo) lo = v; if (n == 1 || v > hi) hi = v } END { if (n) print lo, hi }' "${RESULTS}"
}

# Range verdict: $1 base "lo hi", $2 cand "lo hi", $3 higher|lower (which is better).
range_verdict() {
  awk -v b="$1" -v c="$2" -v dir="$3" 'BEGIN {
    if (split(b, B, " ") < 2 || split(c, C, " ") < 2) { print "n/a"; exit }
    above = (C[1] > B[2]); below = (C[2] < B[1])
    if (!above && !below) print "~"
    else if ((above && dir == "higher") || (below && dir == "lower")) print "BETTER"
    else print "WORSE" }'
}

# Median of the values for (test, tag, metric).
median() {
  awk -F'\t' -v t="$1" -v g="$2" -v m="$3" '$1 == t && $2 == g && $4 == m && $5 != "" { print $5 }' "${RESULTS}" \
    | sort -g | awk '{ v[NR] = $1 } END { if (NR == 0) print ""; else if (NR % 2) print v[(NR + 1) / 2]; else print (v[NR / 2] + v[NR / 2 + 1]) / 2 }'
}

#################
# PARSE OPTIONS #
#################
while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --base) BASE_TAG=$2; shift ;;
    --cand) CAND_TAG=$2; shift ;;
    --rounds) ROUNDS=$2; shift ;;
    --test) TESTS+=("${2#tests/}"); shift ;;
    *) usage; die "unknown option: $1" ;;
  esac
  shift
done
[ ${#TESTS[@]} -eq 0 ] && TESTS=("${DEFAULT_TESTS[@]}")
[[ "${ROUNDS}" =~ ^[1-9][0-9]*$ ]] || die "invalid --rounds '${ROUNDS}'"

#############
# EXECUTION #
#############
for t in docker curl jq h2load awk ss; do command -v $t &>/dev/null || die "missing tool '$t'"; done
[ -n "${LOAD_CPUS}" ] && { command -v taskset &>/dev/null || die "missing tool 'taskset' (LOAD_CPUS)"; }
for tag in "${BASE_TAG}" "${CAND_TAG}"; do
  docker image inspect "${IMAGE}:${tag}" &>/dev/null || die "image ${IMAGE}:${tag} not found"
done
for p in ${ADMIN_PORT} ${TRAFFIC_PORT} ${PROM_PORT} $((ADMIN_PORT + 1)) $((PROM_PORT + 1)); do
  port_busy "$p" && die "port $p is busy (override ADMIN_PORT/TRAFFIC_PORT/PROM_PORT)"
done
for t in "${TESTS[@]}"; do [ -d "${SCR_DIR}/tests/${t}" ] || die "profile tests/${t} not found"; done

OUT_DIR=${OUT_DIR:-${SCR_DIR}/compare-reports/$(date '+%Y%m%d_%H%M%S')_${BASE_TAG}_vs_${CAND_TAG}}
mkdir -p "${OUT_DIR}" || die "cannot create ${OUT_DIR}"
RESULTS="${OUT_DIR}/results.tsv"
echo -e "test\ttag\tround\tmetric\tvalue" > "${RESULTS}"
trap 'stop_server; docker rm -f h2agent-bench-client &>/dev/null' EXIT

echo "Comparing ${IMAGE}:${BASE_TAG} (base) vs ${IMAGE}:${CAND_TAG} (candidate), ${ROUNDS} round(s)"
echo "Profiles: ${TESTS[*]}"
echo "Output:   ${OUT_DIR}"
FAILED=0
for round in $(seq 1 "${ROUNDS}"); do
  order=("${BASE_TAG}" "${CAND_TAG}")
  [ $((round % 2)) -eq 0 ] && order=("${CAND_TAG}" "${BASE_TAG}")  # alternate to cancel drift
  for test in "${TESTS[@]}"; do
    for tag in "${order[@]}"; do
      echo "[round ${round}/${ROUNDS}] ${test} @ ${tag}"
      run_one "${test}" "${tag}" "${round}" || FAILED=1
    done
  done
done

# Summary: medians, delta and range verdict (see usage).
SUMMARY="${OUT_DIR}/summary.md"
VERDICT_RC=0
{
  echo "# ${BASE_TAG} vs ${CAND_TAG} (${ROUNDS} round(s), medians; verdict over min-max ranges)"
  echo
  echo "| Profile | Metric | ${BASE_TAG} | ${CAND_TAG} | Delta | Verdict |"
  echo "|---|---|---|---|---|---|"
  for test in "${TESTS[@]}"; do
    metrics=$(awk -F'\t' -v t="${test}" '$1 == t { print $4 }' "${RESULTS}" | awk '!seen[$0]++')
    while IFS= read -r m; do
      [ -z "$m" ] && continue
      b=$(median "${test}" "${BASE_TAG}" "$m"); c=$(median "${test}" "${CAND_TAG}" "$m")
      delta=$(awk -v b="$b" -v c="$c" 'BEGIN { if (b == "" || c == "" || b == 0) print "n/a"; else printf "%+.1f%%", (c - b) * 100 / b }')
      verdict=
      case "$m" in
        req/s|cps)
          verdict=$(range_verdict "$(range_of "${test}" "${BASE_TAG}" "$m")" "$(range_of "${test}" "${CAND_TAG}" "$m")" higher) ;;
        cpu/request*|cpu/provision*)
          verdict=$(range_verdict "$(range_of "${test}" "${BASE_TAG}" "$m")" "$(range_of "${test}" "${CAND_TAG}" "$m")" lower) ;;
        non-2xx+failed)
          verdict=$([ "$b" = "$c" ] && echo "OK" || echo "INVALID") ;;
      esac
      [ "${verdict}" = "WORSE" -o "${verdict}" = "INVALID" ] && VERDICT_RC=2
      echo "| ${test} | ${m} | ${b} | ${c} | ${delta} | ${verdict} |"
    done <<< "${metrics}"
  done
  echo
  echo "Per-run logs and reports: ${OUT_DIR}"
} > "${SUMMARY}"
echo; cat "${SUMMARY}"
[ ${FAILED} -ne 0 ] && echo "WARNING: some runs failed (see logs)" && VERDICT_RC=2
exit ${VERDICT_RC}
