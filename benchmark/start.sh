#!/bin/bash
# Prepend variables: H2AGENT__ADMIN_PORT, H2AGENT__TRAFFIC_PORT and H2AGENT__PROMETHEUS_PORT

#############
# VARIABLES #
#############
SCR_DIR=$(readlink -f "$(dirname "$0")")
TESTS_DIR=${SCR_DIR}/tests
ANALYSIS_DIR=${SCR_DIR}/analysis
DEFAULTS=
TEST_NAME=tests/server/default

# Filesystem operations:
rm -f /tmp/h2agent_benchmark_timestamp_usecs.log

# Default values
H2AGENT__FILE_MANAGER_ENABLE_READ_CACHE_CONFIGURATION__dflt=true
H2AGENT__SERVER_TRAFFIC_IGNORE_REQUEST_BODY_CONFIGURATION__dflt=false
H2AGENT__SERVER_TRAFFIC_DYNAMIC_REQUEST_BODY_ALLOCATION_CONFIGURATION__dflt=false
H2AGENT__DATA_STORAGE_CONFIGURATION__dflt=discard-all
H2AGENT__DATA_PURGE_CONFIGURATION__dflt=disable-purge
H2AGENT__BIND_ADDRESS__dflt=0.0.0.0
H2AGENT__RESPONSE_DELAY_MS__dflt=0

H2LOAD__ITERATIONS__dflt=100000
H2LOAD__CLIENTS__dflt=1
#H2LOAD__THREADS__dflt=1
H2LOAD__CONCURRENT_STREAMS__dflt=100
#H2LOAD__EXTRA_ARGS="-w 20 -W 20" # max=30 by default

H2CLIENT__DATA_STORAGE_CONFIGURATION__dflt=discard-all
H2CLIENT__DATA_PURGE_CONFIGURATION__dflt=disable-purge
H2CLIENT__CPS__dflt=10000
H2CLIENT__ITERATIONS__dflt=100000

# Prepend values
H2AGENT__ADMIN_PORT=${H2AGENT__ADMIN_PORT:-8074}
H2AGENT__TRAFFIC_PORT=${H2AGENT__TRAFFIC_PORT:-8000}
H2AGENT__PROMETHEUS_PORT=${H2AGENT__PROMETHEUS_PORT:-8080}

#############
# FUNCTIONS #
#############
usage() {
  cat << EOF
H2agent benchmark script

Usage: $0 [-h|--help] [-y] [--test <name>] [--list] [--delay <ms>]

          -h|--help:      this help.
          -y:             assume defaults for all the questions.
          --test <path>:  select test profile (e.g. tests/server/default, tests/client/default).
          --list:         list available test profiles and exit.
          --delay <ms>:   override response delay in milliseconds (server mode).

Test profiles are organized under benchmark/tests/:

  tests/server/<name>/   Server benchmarks (h2load → h2agent server under test)
    test.json              metadata (description, requestMethod, requestUri, requestBody, requestHeaders)
    server-provision.json  server provision configuration
    global-variable.json   (optional) global variables

  tests/client/<name>/   Client benchmarks (h2agent client under test → h2agent server mock)
    test.json              metadata (description)
    client-provision.json  client provision configuration
    server-provision.json  server mock provision (fixture)
    client-endpoint.json   (optional) client endpoint configuration
    global-variable.json   (optional) global variables

Mode is auto-detected from the profile directory (server/ or client/).
Reports are generated as markdown under the profile's reports/ subdirectory.

Examples:

  $0 --list
  $0 -y                                              # tests/server/default
  $0 --test tests/server/light -y                    # specific server benchmark
  $0 --test tests/client/default -y                  # client benchmark
  $0 --test tests/server/light --delay 1000 -y       # server benchmark with delay
EOF
}

list_tests() {
  echo -e "\nAvailable test profiles:\n"
  for mode in server client; do
    local mode_dir="${TESTS_DIR}/${mode}"
    [ ! -d "$mode_dir" ] && continue
    echo "  [${mode}]"
    for dir in "${mode_dir}"/*/; do
      [ ! -f "${dir}/test.json" ] && continue
      local name=$(basename "$dir")
      local desc=$(jq -r '.description // "no description"' "${dir}/test.json")
      printf "    tests/%-23s %s\n" "${mode}/${name}" "${desc}"
    done
    echo
  done
}

# $1: method; $2: uri path; $3: optional expected status; $4: optional request body file
h2a_admin_curl() {
  local method=$1
  local uri=$2
  local expected_status=$3
  local dataFile=$4

  local s_dataFile_option=
  [ -n "${dataFile}" ] && s_dataFile_option="-d@${dataFile}"

  status=$(curl -s -w "%{http_code}" --http2-prior-knowledge -o ${TMP_DIR}/curl.output -H 'content-type: application/json' -X${method} ${s_dataFile_option} "http://${H2AGENT__BIND_ADDRESS}:${H2AGENT__ADMIN_PORT}/${uri}")

  if [ -n "${expected_status}" ]; then
    if [ "$status" != "${expected_status}" ]; then
      cat ${TMP_DIR}/curl.output 2>/dev/null || echo -e "\nERROR: check if 'h2agent' is started"
      return 1
    fi
  fi
}

# $1: input data; $2: pipe-separated list of accepted values
check_valid_values() {
  [ -z "$2" ] && return 0
  echo $2 | tr '|' '\n' | egrep -q "^${1}$"
  local rc=$?
  [ $rc -ne 0 ] && echo -e "\nInvalid value '$1' !"
  return ${rc}
}

# $1: what; $2: output; $3: space-separated list of accepted values
read_value() {
  local what=$1
  local -n output=$2
  local validValues=$3

  local s_supported=
  [ -n "${validValues}" ] && s_supported=" (${validValues})"

  local default=$(eval echo \$$2__dflt)
  echo
  [ -n "${output}" ] && echo "Prepended ${what}: ${2}=${output}" && check_valid_values "${output}" "${validValues}" && return $?
  echo -en "Input ${what}${s_supported}\n (or set '$2' to be non-interactive) [${default}]: "
  [ -z "${DEFAULTS}" ] && read output
  [ -z "${output}" ] && output=${default} && echo ${output}

  check_valid_values "${output}" "${validValues}"
}

save_metadata() {
  cat > "${TMP_DIR}/metadata.env" << METADATA
TEST_NAME="${TEST_NAME}"
TEST_DESC="${TEST_DESC}"
BENCH_MODE="${BENCH_MODE}"
ST_REQUEST_METHOD="${ST_REQUEST_METHOD}"
ST_REQUEST_URI="${ST_REQUEST_URI}"
H2AGENT__RESPONSE_DELAY_MS="${H2AGENT__RESPONSE_DELAY_MS}"
H2AGENT__DATA_STORAGE_CONFIGURATION="${H2AGENT__DATA_STORAGE_CONFIGURATION}"
H2AGENT__DATA_PURGE_CONFIGURATION="${H2AGENT__DATA_PURGE_CONFIGURATION}"
H2LOAD__ITERATIONS="${H2LOAD__ITERATIONS}"
H2LOAD__CLIENTS="${H2LOAD__CLIENTS}"
H2LOAD__THREADS="${H2LOAD__THREADS}"
H2LOAD__CONCURRENT_STREAMS="${H2LOAD__CONCURRENT_STREAMS}"
H2CLIENT__CPS="${H2CLIENT__CPS}"
H2CLIENT__ITERATIONS="${H2CLIENT__ITERATIONS}"
ELAPSED_MS="${ELAPSED_MS}"
ACTUAL_CPS="${ACTUAL_CPS}"
METADATA
}

generate_report() {
  local reports_dir="${TEST_DIR}/reports"
  mkdir -p "${reports_dir}"
  local timestamp=$(date '+%Y%m%d_%H%M%S')
  local report_file="${reports_dir}/${timestamp}_${BENCH_MODE}.md"

  save_metadata
  bash "${ANALYSIS_DIR}/report.sh" "${TMP_DIR}" "${report_file}" "${TMP_DIR}/launcher.output"
}

# Resolve test profile: expects tests/server/<name> or tests/client/<name>
resolve_test() {
  # Strip trailing slash (bash completion may add it)
  TEST_NAME="${TEST_NAME%/}"

  # Validate format: tests/<mode>/<name>
  local rest="${TEST_NAME#tests/}"
  if [ "$rest" = "$TEST_NAME" ]; then
    echo "ERROR: test profile must start with tests/ (e.g. tests/server/default)"
    list_tests; exit 1
  fi

  local mode="${rest%%/*}"
  local name="${rest#*/}"
  if [ "$mode" != "server" -a "$mode" != "client" ] || [ -z "$name" ]; then
    echo "ERROR: test profile must be tests/server/<name> or tests/client/<name>"
    list_tests; exit 1
  fi

  TEST_DIR="${SCR_DIR}/${TEST_NAME}"
  if [ ! -d "$TEST_DIR" ]; then
    echo "ERROR: test profile '${TEST_NAME}' not found"
    list_tests; exit 1
  fi

  BENCH_MODE=${mode}
  TEST_NAME=${name}
}

#################
# PARSE OPTIONS #
#################
while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    -y) DEFAULTS=true ;;
    --list) list_tests; exit 0 ;;
    --test) TEST_NAME=$2; shift ;;
    --delay) H2AGENT__RESPONSE_DELAY_MS=$2; shift ;;
    *) echo "Unknown option: $1"; usage; exit 1 ;;
  esac
  shift
done

#############
# EXECUTION #
#############
echo
cd ${SCR_DIR}

# Requirements
which jq &>/dev/null || { echo "Required 'jq' tool (https://stedolan.github.io/jq/)" ; exit 1 ; }

# Resolve and validate test profile
resolve_test
[ ! -f "${TEST_DIR}/test.json" ] && echo "ERROR: missing test.json in ${TEST_DIR}/" && exit 1

TEST_DESC=$(jq -r '.description // ""' "${TEST_DIR}/test.json")

echo "Test profile: ${TEST_NAME} [${BENCH_MODE}]"
echo "Description:  ${TEST_DESC}"

# Validate: server profiles must not have client-provision, client profiles must not have server-provision as primary
if [ "${BENCH_MODE}" = "server" ]; then
  [ -f "${TEST_DIR}/client-provision.json" ] && echo "ERROR: server test profile must not contain client-provision.json" && exit 1
  [ ! -f "${TEST_DIR}/server-provision.json" ] && echo "ERROR: missing server-provision.json in ${TEST_DIR}/" && exit 1
elif [ "${BENCH_MODE}" = "client" ]; then
  [ ! -f "${TEST_DIR}/client-provision.json" ] && echo "ERROR: missing client-provision.json in ${TEST_DIR}/" && exit 1
fi

TMP_DIR=$(mktemp -d)
trap "rm -rf ${TMP_DIR}" EXIT

# Common: configure h2agent address
read_value "H2agent endpoint address" H2AGENT__BIND_ADDRESS

# Source monitor functions
source "${ANALYSIS_DIR}/monitor.sh"
PROMETHEUS_URL="http://${H2AGENT__BIND_ADDRESS}:${H2AGENT__PROMETHEUS_PORT}/metrics"

###############################################
# SERVER MODE: h2load → h2agent under test
###############################################
if [ "${BENCH_MODE}" = "server" ]; then

  which h2load &>/dev/null || { echo "Required 'h2load' tool (https://nghttp2.org/documentation/h2load-howto.html)" ; exit 1 ; }

  ST_REQUEST_METHOD=$(jq -r '.requestMethod // "POST"' "${TEST_DIR}/test.json")
  ST_REQUEST_URI=$(jq -r '.requestUri // "/app/v1/benchmark/echo"' "${TEST_DIR}/test.json")
  ST_REQUEST_BODY=$(jq -c '.requestBody // empty' "${TEST_DIR}/test.json" 2>/dev/null)
  ST_REQUEST_HEADERS=$(jq -c '.requestHeaders // empty' "${TEST_DIR}/test.json" 2>/dev/null)

  echo "Method:       ${ST_REQUEST_METHOD}"
  echo "URI:          ${ST_REQUEST_URI}"
  [ -n "${ST_REQUEST_BODY}" ] && echo "Body:         ${ST_REQUEST_BODY}"
  [ -n "${ST_REQUEST_HEADERS}" ] && echo "Headers:      ${ST_REQUEST_HEADERS}"

  # Server configuration options
  read_value "File manager configuration to enable read cache" H2AGENT__FILE_MANAGER_ENABLE_READ_CACHE_CONFIGURATION "true|false" || exit 1
  read_value "Server configuration to ignore request body" H2AGENT__SERVER_TRAFFIC_IGNORE_REQUEST_BODY_CONFIGURATION "true|false" || exit 1
  read_value "Server configuration to perform dynamic request body allocation" H2AGENT__SERVER_TRAFFIC_DYNAMIC_REQUEST_BODY_ALLOCATION_CONFIGURATION "true|false" || exit 1
  read_value "Server data storage configuration" H2AGENT__DATA_STORAGE_CONFIGURATION "discard-all|discard-history|keep-all" || exit 1
  read_value "Server data purge configuration" H2AGENT__DATA_PURGE_CONFIGURATION "enable-purge|disable-purge" || exit 1
  if [ -z "${H2AGENT__RESPONSE_DELAY_MS}" ] || [ "${H2AGENT__RESPONSE_DELAY_MS}" = "0" ]; then
    read_value "H2agent response delay in milliseconds" H2AGENT__RESPONSE_DELAY_MS
  fi
  if [ "${H2AGENT__RESPONSE_DELAY_MS:-0}" -ne 0 ]; then
    echo -e "\nResponse delay: ${H2AGENT__RESPONSE_DELAY_MS}ms"
    H2LOAD__ITERATIONS__dflt=$(( 1000000 / H2AGENT__RESPONSE_DELAY_MS ))
    [ "${H2LOAD__ITERATIONS__dflt}" -lt 100 ] && H2LOAD__ITERATIONS__dflt=100
  fi

  # Prepare server provision (apply delay override if non-zero)
  if [ "${H2AGENT__RESPONSE_DELAY_MS}" != "0" ]; then
    jq --argjson delay "${H2AGENT__RESPONSE_DELAY_MS}" '[.[] | .responseDelayMs = $delay]' "${TEST_DIR}/server-provision.json" > ${TMP_DIR}/server-provision.json
  else
    cp "${TEST_DIR}/server-provision.json" ${TMP_DIR}/server-provision.json
  fi

  # Configure server
  echo '{"algorithm":"FullMatching"}' > ${TMP_DIR}/server-matching.json
  h2a_admin_curl DELETE admin/v1/server-provision
  h2a_admin_curl DELETE admin/v1/global-variable
  h2a_admin_curl POST admin/v1/server-matching 201 ${TMP_DIR}/server-matching.json || exit 1
  h2a_admin_curl POST admin/v1/server-provision 201 ${TMP_DIR}/server-provision.json || exit 1
  h2a_admin_curl PUT "admin/v1/files/configuration?readCache=${H2AGENT__FILE_MANAGER_ENABLE_READ_CACHE_CONFIGURATION}" 200 || exit 1

  case ${H2AGENT__SERVER_TRAFFIC_IGNORE_REQUEST_BODY_CONFIGURATION} in
    false) RECEIVE_REQUEST_BODY=true ;; true) RECEIVE_REQUEST_BODY=false ;; esac
  case ${H2AGENT__SERVER_TRAFFIC_DYNAMIC_REQUEST_BODY_ALLOCATION_CONFIGURATION} in
    false) PRE_RESERVE_REQUEST_BODY=true ;; true) PRE_RESERVE_REQUEST_BODY=false ;; esac
  h2a_admin_curl PUT "admin/v1/server/configuration?receiveRequestBody=${RECEIVE_REQUEST_BODY}&preReserveRequestBody=${PRE_RESERVE_REQUEST_BODY}" 200 || exit 1
  echo -e "\nServer configuration:"
  h2a_admin_curl GET "admin/v1/server/configuration" || exit 1
  cat ${TMP_DIR}/curl.output

  case ${H2AGENT__DATA_STORAGE_CONFIGURATION} in
    discard-all) DISCARD_DATA=true; DISCARD_DATA_HISTORY=true ;;
    discard-history) DISCARD_DATA=false; DISCARD_DATA_HISTORY=true ;;
    keep-all) DISCARD_DATA=false; DISCARD_DATA_HISTORY=false ;; esac
  case ${H2AGENT__DATA_PURGE_CONFIGURATION} in
    enable-purge) DISABLE_PURGE=false ;; disable-purge) DISABLE_PURGE=true ;; esac
  h2a_admin_curl PUT "admin/v1/server-data/configuration?discard=${DISCARD_DATA}&discardKeyHistory=${DISCARD_DATA_HISTORY}&disablePurge=${DISABLE_PURGE}" 200 || exit 1
  echo -e "\nServer data configuration:"
  h2a_admin_curl GET "admin/v1/server-data/configuration" || exit 1
  cat ${TMP_DIR}/curl.output
  echo -en "\n\nRemoving current server data information ... "
  h2a_admin_curl DELETE "admin/v1/server-data"
  echo "done !"

  # Global variables
  [ -f "${TEST_DIR}/global-variable.json" ] && h2a_admin_curl POST admin/v1/global-variable 201 "${TEST_DIR}/global-variable.json" || true

  # h2load parameters
  read_value "Number of h2load iterations" H2LOAD__ITERATIONS
  read_value "Number of h2load clients" H2LOAD__CLIENTS
  H2LOAD__THREADS__dflt=${H2LOAD__CLIENTS}
  read_value "Number of h2load threads" H2LOAD__THREADS
  read_value "Number of h2load concurrent streams" H2LOAD__CONCURRENT_STREAMS

  # Build request
  ST_REQUEST_URL=$(echo ${ST_REQUEST_URI} | sed -e 's/^\///')
  s_DATA_OPT=
  s_HEADER_OPTS=
  if [ -n "${ST_REQUEST_BODY}" ]; then
    echo "${ST_REQUEST_BODY}" > ${TMP_DIR}/request.json
    s_DATA_OPT="-d ${TMP_DIR}/request.json"
  elif [ "${ST_REQUEST_METHOD}" != "GET" ]; then
    s_DATA_OPT="-d /dev/null"
  fi
  [ -n "${ST_REQUEST_HEADERS}" ] && {
    for key in $(echo "${ST_REQUEST_HEADERS}" | jq -r 'keys[]'); do
      val=$(echo "${ST_REQUEST_HEADERS}" | jq -r --arg k "$key" '.[$k]')
      s_HEADER_OPTS="${s_HEADER_OPTS} -H '${key}: ${val}'"
    done
  }

  # Start monitor (target: h2agent server)
  H2AGENT_PID=$(pgrep -x h2agent | head -1)
  [ -n "$H2AGENT_PID" ] && monitor_start "$H2AGENT_PID" "$TMP_DIR" "$PROMETHEUS_URL" && echo -e "\nMonitoring h2agent server (PID ${H2AGENT_PID})..."

  # Run h2load
  echo
  set -x
  eval time h2load ${H2LOAD__EXTRA_ARGS} -t${H2LOAD__THREADS} -n${H2LOAD__ITERATIONS} -c${H2LOAD__CLIENTS} -m${H2LOAD__CONCURRENT_STREAMS} ${s_HEADER_OPTS} http://${H2AGENT__BIND_ADDRESS}:${H2AGENT__TRAFFIC_PORT}/${ST_REQUEST_URL} ${s_DATA_OPT} 2>&1 | tee ${TMP_DIR}/launcher.output
  set +x

  # Stop monitor
  [ -n "$H2AGENT_PID" ] && monitor_stop "$TMP_DIR" "$PROMETHEUS_URL"

###############################################
# CLIENT MODE: h2agent client under test → h2agent server mock
###############################################
# A dedicated h2agent process is started for the client role (ports +1).
# The existing h2agent (from run.sh) acts as the server mock (fixture).
# This avoids io_context contention between client and server.
###############################################
elif [ "${BENCH_MODE}" = "client" ]; then

  H2AGENT_DCK_IMG=${H2AGENT_DCK_IMG:-ghcr.io/testillano/h2agent}
  H2AGENT_DCK_TAG=${H2AGENT_DCK_TAG:-latest}
  CLIENT_DCK_NAME="h2agent-bench-client"

  CLIENT_ADMIN_PORT=$((H2AGENT__ADMIN_PORT + 1))
  CLIENT_PROMETHEUS_PORT=$((H2AGENT__PROMETHEUS_PORT + 1))
  CLIENT_PROMETHEUS_URL="http://${H2AGENT__BIND_ADDRESS}:${CLIENT_PROMETHEUS_PORT}/metrics"
  NPROC=$(nproc)
  CLIENT_WORKERS=$(( NPROC / 2 > 2 ? NPROC / 2 : 2 ))

  # Server mock setup (fixture) — on the MAIN h2agent
  h2a_admin_curl DELETE admin/v1/server-provision
  h2a_admin_curl DELETE admin/v1/global-variable
  if [ -f "${TEST_DIR}/server-provision.json" ]; then
    echo '{"algorithm":"FullMatching"}' > ${TMP_DIR}/server-matching.json
    h2a_admin_curl POST admin/v1/server-matching 201 ${TMP_DIR}/server-matching.json || exit 1
    h2a_admin_curl POST admin/v1/server-provision 201 "${TEST_DIR}/server-provision.json" || exit 1
    echo "Server mock configured (fixture) on port ${H2AGENT__ADMIN_PORT}"
  fi
  [ -f "${TEST_DIR}/global-variable.json" ] && h2a_admin_curl POST admin/v1/global-variable 201 "${TEST_DIR}/global-variable.json" || true

  # Client parameters
  read_value "Client data storage configuration" H2CLIENT__DATA_STORAGE_CONFIGURATION "discard-all|discard-history|keep-all" || exit 1
  read_value "Client data purge configuration" H2CLIENT__DATA_PURGE_CONFIGURATION "enable-purge|disable-purge" || exit 1
  read_value "Calls per second" H2CLIENT__CPS
  read_value "Number of iterations" H2CLIENT__ITERATIONS

  # Start dedicated client h2agent (no traffic server, just client + admin + prometheus)
  echo -e "\nStarting client h2agent (admin=${CLIENT_ADMIN_PORT}, prometheus=${CLIENT_PROMETHEUS_PORT}, workers=${CLIENT_WORKERS})..."
  docker run --rm -d --network=host --name ${CLIENT_DCK_NAME} -u $(id -u) \
    ${H2AGENT_DCK_IMG}:${H2AGENT_DCK_TAG} \
    --admin-port ${CLIENT_ADMIN_PORT} \
    --traffic-server-port 0 \
    --traffic-client-worker-threads ${CLIENT_WORKERS} \
    --prometheus-port ${CLIENT_PROMETHEUS_PORT} \
    --prometheus-response-delay-seconds-histogram-boundaries "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3" \
    --log-level Warning \
    --verbose > /dev/null || { echo "ERROR: failed to start client h2agent container"; exit 1; }
  trap "docker rm -f ${CLIENT_DCK_NAME} 2>/dev/null; rm -rf ${TMP_DIR}" EXIT

  # Wait for client h2agent to be ready
  for i in $(seq 1 30); do
    curl -sf --http2-prior-knowledge "http://${H2AGENT__BIND_ADDRESS}:${CLIENT_ADMIN_PORT}/admin/v1/configuration" -o /dev/null 2>/dev/null && break
    sleep 0.1
  done
  CLIENT_H2AGENT_PID=$(docker inspect -f '{{.State.Pid}}' ${CLIENT_DCK_NAME} 2>/dev/null)
  echo "Client h2agent ready (container=${CLIENT_DCK_NAME}, PID=${CLIENT_H2AGENT_PID}, workers=${CLIENT_WORKERS})"

  # Helper: admin curl targeting the CLIENT h2agent
  client_admin_curl() {
    local method=$1 uri=$2 expected_status=$3 dataFile=$4
    local s_dataFile_option=
    [ -n "${dataFile}" ] && s_dataFile_option="-d@${dataFile}"
    status=$(curl -s -w "%{http_code}" --http2-prior-knowledge -o ${TMP_DIR}/curl.output -H 'content-type: application/json' -X${method} ${s_dataFile_option} "http://${H2AGENT__BIND_ADDRESS}:${CLIENT_ADMIN_PORT}/${uri}")
    if [ -n "${expected_status}" ] && [ "$status" != "${expected_status}" ]; then
      cat ${TMP_DIR}/curl.output 2>/dev/null
      return 1
    fi
  }

  # Configure client data
  case ${H2CLIENT__DATA_STORAGE_CONFIGURATION} in
    discard-all) CLIENT_DISCARD_DATA=true; CLIENT_DISCARD_DATA_HISTORY=true ;;
    discard-history) CLIENT_DISCARD_DATA=false; CLIENT_DISCARD_DATA_HISTORY=true ;;
    keep-all) CLIENT_DISCARD_DATA=false; CLIENT_DISCARD_DATA_HISTORY=false ;; esac
  case ${H2CLIENT__DATA_PURGE_CONFIGURATION} in
    enable-purge) CLIENT_DISABLE_PURGE=false ;; disable-purge) CLIENT_DISABLE_PURGE=true ;; esac
  client_admin_curl PUT "admin/v1/client-data/configuration?discard=${CLIENT_DISCARD_DATA}&discardKeyHistory=${CLIENT_DISCARD_DATA_HISTORY}&disablePurge=${CLIENT_DISABLE_PURGE}" 200 || exit 1
  echo -e "\nClient data configuration:"
  client_admin_curl GET "admin/v1/client-data/configuration" || exit 1
  cat ${TMP_DIR}/curl.output

  # Configure client endpoint (pointing to main h2agent's traffic port)
  if [ -f "${TEST_DIR}/client-endpoint.json" ]; then
    client_admin_curl POST admin/v1/client-endpoint 201 "${TEST_DIR}/client-endpoint.json" || exit 1
  else
    echo "{\"id\":\"server\",\"host\":\"${H2AGENT__BIND_ADDRESS}\",\"port\":${H2AGENT__TRAFFIC_PORT}}" > ${TMP_DIR}/client-endpoint.json
    client_admin_curl POST admin/v1/client-endpoint 201 ${TMP_DIR}/client-endpoint.json || exit 1
  fi

  # Configure client provision
  client_admin_curl POST admin/v1/client-provision 201 "${TEST_DIR}/client-provision.json" || exit 1

  PROVISION_ID=$(jq -r 'if type == "array" then .[0].id else .id end' "${TEST_DIR}/client-provision.json")
  echo -e "\nClient provision '${PROVISION_ID}' configured"

  # Start monitor (target: client h2agent container process)
  [ -n "$CLIENT_H2AGENT_PID" ] && monitor_start "$CLIENT_H2AGENT_PID" "$TMP_DIR" "$CLIENT_PROMETHEUS_URL"
  echo -e "\nMonitoring client h2agent (PID ${CLIENT_H2AGENT_PID})..."

  # Trigger timer-based load
  EXPECTED_SECS=$(( (H2CLIENT__ITERATIONS + H2CLIENT__CPS - 1) / H2CLIENT__CPS ))
  echo
  echo "Triggering ${H2CLIENT__ITERATIONS} provisions at ${H2CLIENT__CPS} cps (~${EXPECTED_SECS}s expected)..."
  START_NS=$(date +%s%N)
  client_admin_curl GET "admin/v1/client-provision/${PROVISION_ID}?sequenceBegin=1&sequenceEnd=${H2CLIENT__ITERATIONS}&cps=${H2CLIENT__CPS}" || exit 1
  cat ${TMP_DIR}/curl.output
  echo

  # Wait for completion: poll prometheus for expected number of sent requests
  STEPS_PER_CHAIN=$(jq 'length' "${TEST_DIR}/client-provision.json")
  EXPECTED_REQUESTS=$((H2CLIENT__ITERATIONS * STEPS_PER_CHAIN))
  TIMEOUT_SECS=$(( EXPECTED_SECS * 10 + 10 ))
  echo "Waiting for completion (~${EXPECTED_REQUESTS} requests = ${H2CLIENT__ITERATIONS}×${STEPS_PER_CHAIN} steps, timeout ${TIMEOUT_SECS}s)..."
  sleep 0.5  # let the timer start
  while true; do
    sleep 0.5
    sent=$(curl -sf "${CLIENT_PROMETHEUS_URL}" 2>/dev/null | awk '/^h2agent_traffic_client_observed_requests_sents_counter[{ ]/{s+=$NF} END{printf "%d",s}')
    [ "${sent:-0}" -ge "${EXPECTED_REQUESTS}" ] && break
    now=$(date +%s%N)
    [ $(( (now - START_NS) / 1000000000 )) -ge ${TIMEOUT_SECS} ] && echo "Timeout! (sent=${sent}/${EXPECTED_REQUESTS})" && break
  done
  END_NS=$(date +%s%N)

  ELAPSED_MS=$(( (END_NS - START_NS) / 1000000 ))
  ACTUAL_CPS=$(( H2CLIENT__ITERATIONS * 1000 / ELAPSED_MS ))
  echo
  echo "Completed: ${H2CLIENT__ITERATIONS} provisions in ${ELAPSED_MS}ms (~${ACTUAL_CPS} cps)"
  echo "Completed: ${H2CLIENT__ITERATIONS} provisions in ${ELAPSED_MS}ms (~${ACTUAL_CPS} cps)" > ${TMP_DIR}/launcher.output

  # Stop monitor and remove client h2agent container
  monitor_stop "$TMP_DIR" "$CLIENT_PROMETHEUS_URL"
  docker rm -f ${CLIENT_DCK_NAME} 2>/dev/null
  echo "Client h2agent stopped"
fi

# Generate markdown report
generate_report
