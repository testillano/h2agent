#!/bin/bash
# Prepend variables: H2AGENT__ADMIN_PORT and H2AGENT__TRAFFIC_PORT

#############
# VARIABLES #
#############
SCR_DIR=$(readlink -f "$(dirname "$0")")
DEFAULTS=
[ "$1" = "-y" ] && DEFAULTS=true

# Filesystem operations:
# Read
H2AGENT__FILE_MANAGER_ENABLE_READ_CACHE_CONFIGURATION__dflt=true
# Write
rm -f /tmp/h2agent_benchmark_timestamp_usecs.log # long-term file at provision does not remove possible existing one

# Default values
H2AGENT_VALIDATE_SCHEMAS__dflt=n
H2AGENT_SCHEMA__dflt=schema.json
H2AGENT_SERVER_MATCHING__dflt=server-matching.json
H2AGENT_SERVER_PROVISION__dflt=server-provision.json
H2AGENT_GLOBAL_VARIABLE__dflt=global-variable.json
H2AGENT__SERVER_TRAFFIC_IGNORE_REQUEST_BODY_CONFIGURATION__dflt=false
H2AGENT__SERVER_TRAFFIC_DYNAMIC_REQUEST_BODY_ALLOCATION_CONFIGURATION__dflt=false
H2AGENT__DATA_STORAGE_CONFIGURATION__dflt=discard-all
H2AGENT__DATA_PURGE_CONFIGURATION__dflt=disable-purge
H2AGENT__BIND_ADDRESS__dflt=0.0.0.0
H2AGENT__RESPONSE_DELAY_MS__dflt=0

ST_REQUEST_METHOD__dflt="POST"
ST_REQUEST_URL__dflt="/app/v1/load-test/v1/id-21"
ST_LAUNCHER__dflt=h2load

H2LOAD__ITERATIONS__dflt=100000
H2LOAD__CLIENTS__dflt=1
#H2LOAD__THREADS__dflt=1
H2LOAD__CONCURRENT_STREAMS__dflt=100
#H2LOAD__EXTRA_ARGS="-w 20 -W 20" # max=30 by default

HERMES__RPS__dflt=5000
HERMES__DURATION__dflt=10
HERMES__EXPECTED_RESPONSE_CODE__dflt=200

H2CLIENT__ADMIN_PORT__dflt=8075
H2CLIENT__PROMETHEUS_PORT__dflt=8081
H2CLIENT__RPS__dflt=10000
H2CLIENT__ITERATIONS__dflt=100000

# Hermes image
HERMES_IMG=jgomezselles/hermes:0.0.2

ST_REQUEST_BODY__dflt='{"id":"1a8b8863","name":"Ada Lovelace","email":"ada@geemail.com","bio":"First programmer. No big deal.","age":198,"avatar":"http://en.wikipedia.org/wiki/File:Ada_lovelace.jpg"}'
ST_REQUEST_BODY=${ST_REQUEST_BODY-${ST_REQUEST_BODY__dflt}}

# Prepend values
H2AGENT__ADMIN_PORT=${H2AGENT__ADMIN_PORT:-8074}
H2AGENT__TRAFFIC_PORT=${H2AGENT__TRAFFIC_PORT:-8000}

# Common variables
COMMON_VARS="H2AGENT_VALIDATE_SCHEMAS H2AGENT_SCHEMA H2AGENT_SERVER_MATCHING H2AGENT_SERVER_PROVISION H2AGENT_GLOBAL_VARIABLE H2AGENT__FILE_MANAGER_ENABLE_READ_CACHE_CONFIGURATION H2AGENT__SERVER_TRAFFIC_IGNORE_REQUEST_BODY_CONFIGURATION H2AGENT__SERVER_TRAFFIC_DYNAMIC_REQUEST_BODY_ALLOCATION_CONFIGURATION H2AGENT__DATA_STORAGE_CONFIGURATION H2AGENT__DATA_PURGE_CONFIGURATION H2AGENT__BIND_ADDRESS H2AGENT__RESPONSE_DELAY_MS ST_REQUEST_METHOD ST_REQUEST_URL ST_LAUNCHER"

#############
# FUNCTIONS #
#############
usage() {

  cat << EOF
H2agent 'mock server' benchmark script

Usage: $0 [-h|--help] [-y]

          -h|--help: this help.
          -y:        assume defaults for all the questions.

Examples:

1) Test that h2agent delay timers are managed asynchronously for the worker
   thread, freeing the tatsuhiro's nghttp2 io context for an specific stream:

   $ H2AGENT__RESPONSE_DELAY_MS=1000 H2LOAD__ITERATIONS=100 ./start.sh -y

   As m=100, all the iterations (100) must be managed in about 1 second:

   finished in 1.01s, 98.70 req/s, 105.88KB/s
   requests: 100 total, 100 started, 100 done, 100 succeeded, 0 failed, 0 errored, 0 timeout
   status codes: 100 2xx, 0 3xx, 0 4xx, 0 5xx
   traffic: 107.28KB (109859) total, 335B (335) headers (space savings 95.28%), 105.18KB (107700) data
                            min         max        mean          sd    +/- sd
   time for request:      1.01s       1.01s       1.01s      1.96ms    41.00%
   time for connect:      255us       255us       255us         0us   100.00%
   time to 1st byte:      1.01s       1.01s       1.01s         0us   100.00%
   req/s           :      98.75       98.75       98.75        0.00   100.00%

2) Test that no memory leaks arise for the asyncronous timers:

   $ H2AGENT__RESPONSE_DELAY_MS=1 H2LOAD__ITERATIONS=1000000 ./start.sh -y

   %MEM should be stable at 0% (use for example: 'top -H -p $(pgrep h2agent)')

3) Test high load, to get about 14k req/s in 8-cpu machine with default startup
   and these parameters:

   $ H2LOAD__ITERATIONS=100000 ./start.sh -y

   finished in 7.26s, 13779.98 req/s, 14.43MB/s
   requests: 100000 total, 100000 started, 100000 done, 100000 succeeded, 0 failed, 0 errored, 0 timeout
   status codes: 100000 2xx, 0 3xx, 0 4xx, 0 5xx
   traffic: 104.72MB (109809330) total, 293.18KB (300219) headers (space savings 95.77%), 102.71MB (107700000) data
                            min         max        mean          sd    +/- sd
   time for request:     1.89ms     21.95ms      7.20ms      1.35ms    82.44%
   time for connect:      196us       196us       196us         0us   100.00%
   time to 1st byte:    10.30ms     10.30ms     10.30ms         0us   100.00%
   req/s           :   13780.40    13780.40    13780.40        0.00   100.00%
EOF
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

  if [ -n "${expected_status}" ]
  then
    if [ "$status" != "${expected_status}" ]
    then
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

# $@: variable names to show within report header
init_report() {
  REPORT=${REPORT__ref}
  local seq=1 ; while [ -f ${REPORT} ]; do REPORT=${REPORT__ref}.${seq}; seq=$((seq+1)); done

  echo -e "\n----------------------------------------------------" > ${REPORT}
  for var in ${COMMON_VARS} $@
  do
    local val=$(eval echo \$$var)
    echo "${var}=\"\${${var}:-${val}}\" \\"
  done >> ${REPORT}
  echo -e "${PWD}/start.sh" >> ${REPORT}
  echo -e "----------------------------------------------------\n" >> ${REPORT}
}

#############
# EXECUTION #
#############
echo
cd ${SCR_DIR}
[ "$1" = "-h" -o "$1" = "--help" ] && usage && exit 0

# Requirements
which jq &>/dev/null || { echo "Required 'jq' tool (https://stedolan.github.io/jq/)" ; exit 1 ; }

read_value "Validate schemas" H2AGENT_VALIDATE_SCHEMAS "y|n"
if [ "${H2AGENT_VALIDATE_SCHEMAS}" = "y" ]
then
  read_value "Schema configuration" H2AGENT_SCHEMA
  [ ! -f "${H2AGENT_SCHEMA}" ] && echo "ERROR: missing file '${H2AGENT_SCHEMA}' !" && exit 1
fi
read_value "Matching configuration" H2AGENT_SERVER_MATCHING
[ ! -f "${H2AGENT_SERVER_MATCHING}" ] && echo "ERROR: missing file '${H2AGENT_SERVER_MATCHING}' !" && exit 1
read_value "Provision configuration" H2AGENT_SERVER_PROVISION
[ ! -f "${H2AGENT_SERVER_PROVISION}" ] &&  echo "ERROR: missing file '${H2AGENT_SERVER_PROVISION}' !" && exit 1
read_value "Global variable(s) configuration" H2AGENT_GLOBAL_VARIABLE
[ ! -f "${H2AGENT_GLOBAL_VARIABLE}" ] &&  echo "ERROR: missing file '${H2AGENT_GLOBAL_VARIABLE}' !" && exit 1
read_value "File manager configuration to enable read cache" H2AGENT__FILE_MANAGER_ENABLE_READ_CACHE_CONFIGURATION "true|false" || exit 1
read_value "Server configuration to ignore request body" H2AGENT__SERVER_TRAFFIC_IGNORE_REQUEST_BODY_CONFIGURATION "true|false" || exit 1
read_value "Server configuration to perform dynamic request body allocation" H2AGENT__SERVER_TRAFFIC_DYNAMIC_REQUEST_BODY_ALLOCATION_CONFIGURATION "true|false" || exit 1
read_value "Server data storage configuration" H2AGENT__DATA_STORAGE_CONFIGURATION "discard-all|discard-history|keep-all" || exit 1
read_value "Server data purge configuration" H2AGENT__DATA_PURGE_CONFIGURATION "enable-purge|disable-purge" || exit 1
read_value "H2agent endpoint address" H2AGENT__BIND_ADDRESS
read_value "H2agent response delay in milliseconds" H2AGENT__RESPONSE_DELAY_MS
[ ${H2AGENT__RESPONSE_DELAY_MS} -ne 0 ] && H2LOAD__ITERATIONS__dflt=$((H2LOAD__ITERATIONS__dflt/H2AGENT__RESPONSE_DELAY_MS)) # duration correction

read_value "Request method" ST_REQUEST_METHOD "PUT|DELETE|HEAD|POST|GET" || exit 1

if [ "${ST_REQUEST_METHOD}" = "POST" ]
then
  if [ "${ST_REQUEST_BODY}" != "${ST_REQUEST_BODY__dflt}" ]
  then
    echo
    echo "POST request body has been redefined on current shell:"
    echo
    echo "ST_REQUEST_BODY=${ST_REQUEST_BODY}"
    echo
    echo "To restore default request body: unset ST_REQUEST_BODY"
    echo
  else
    cat << EOF

POST request body defaults to:
   ${ST_REQUEST_BODY__dflt}

To override this content from shell, paste the following snippet:

# Define helper function:
random_request() {
   echo "Input desired size in bytes [3000]:"
   read bytes
   [ -z "\${bytes}" ] && bytes=3000
   local size=\$((bytes/15)) # aproximation
   export ST_REQUEST_BODY="{"\$(k=0 ; while [ \$k -lt \$size ]; do k=\$((k+1)); echo -n "\"id\${RANDOM}\":\${RANDOM}"; [ \${k} -lt \$size ] && echo -n "," ; done)"}"
   echo "Random request created has \$(echo \${ST_REQUEST_BODY} | wc -c) bytes (~ \${bytes})"
   echo "If you need as file: echo \\\${ST_REQUEST_BODY} > request-\${bytes}b.json"
}

# Invoke the function:
random_request

EOF
  fi
fi

read_value "Request url" ST_REQUEST_URL
ST_REQUEST_URL=$(echo ${ST_REQUEST_URL} | sed -e 's/^\///') # normalize to not have leading slash

TMP_DIR=$(mktemp -d)
trap "rm -rf ${TMP_DIR}" EXIT

# build provision:
jq --arg replace "${H2AGENT__RESPONSE_DELAY_MS}" '. |= map(if .responseDelayMs == 0 then (.responseDelayMs=($replace | tonumber)) else . end)' ${H2AGENT_SERVER_PROVISION} | \
jq --arg replace "${ST_REQUEST_METHOD}" '. |= map(if .requestMethod == "POST" then (.requestMethod=($replace)) else . end)' | \
jq --arg replace "/${ST_REQUEST_URL}" '. |= map(if .requestUri == "URI" then (.requestUri=($replace)) else . end)' > ${TMP_DIR}/server-provision.json
if [ "${H2AGENT_VALIDATE_SCHEMAS}" != "y" ]
then
  jq 'del (.[0].requestSchemaId,.[0].responseSchemaId)' ${TMP_DIR}/server-provision.json > ${TMP_DIR}/server-provision.json2
  mv ${TMP_DIR}/server-provision.json2 ${TMP_DIR}/server-provision.json
fi

[ "${H2AGENT_VALIDATE_SCHEMAS}" = "y" ] && { h2a_admin_curl POST admin/v1/schema 201 ${H2AGENT_SCHEMA} || exit 1 ; }
h2a_admin_curl POST admin/v1/server-matching 201 ${H2AGENT_SERVER_MATCHING} || exit 1
h2a_admin_curl POST admin/v1/server-provision 201 ${TMP_DIR}/server-provision.json || exit 1

# File manager configuration
h2a_admin_curl PUT "admin/v1/files/configuration?readCache=${H2AGENT__FILE_MANAGER_ENABLE_READ_CACHE_CONFIGURATION}" 200 || exit 1

# Server configuration
case ${H2AGENT__SERVER_TRAFFIC_IGNORE_REQUEST_BODY_CONFIGURATION} in
  false) RECEIVE_REQUEST_BODY=true ;;
  true) RECEIVE_REQUEST_BODY=false ;;
esac
case ${H2AGENT__SERVER_TRAFFIC_DYNAMIC_REQUEST_BODY_ALLOCATION_CONFIGURATION} in
  false) PRE_RESERVE_REQUEST_BODY=true ;;
  true) PRE_RESERVE_REQUEST_BODY=false ;;
esac
h2a_admin_curl PUT "admin/v1/server/configuration?receiveRequestBody=${RECEIVE_REQUEST_BODY}&preReserveRequestBody=${PRE_RESERVE_REQUEST_BODY}" 200 || exit 1
echo -e "\nServer configuration:"
h2a_admin_curl GET "admin/v1/server/configuration" || exit 1
cat ${TMP_DIR}/curl.output

# Server data configuration
case ${H2AGENT__DATA_STORAGE_CONFIGURATION} in
  discard-all) DISCARD_DATA=true; DISCARD_DATA_HISTORY=true ;;
  discard-history) DISCARD_DATA=false; DISCARD_DATA_HISTORY=true ;;
  keep-all) DISCARD_DATA=false; DISCARD_DATA_HISTORY=false ;;
esac
case ${H2AGENT__DATA_PURGE_CONFIGURATION} in
  enable-purge) DISABLE_PURGE=false ;;
  disable-purge) DISABLE_PURGE=true ;;
esac
h2a_admin_curl PUT "admin/v1/server-data/configuration?discard=${DISCARD_DATA}&discardKeyHistory=${DISCARD_DATA_HISTORY}&disablePurge=${DISABLE_PURGE}" 200 || exit 1
echo -e "\nServer data configuration:"
h2a_admin_curl GET "admin/v1/server-data/configuration" || exit 1
cat ${TMP_DIR}/curl.output
echo -en "\n\nRemoving current server data information ... "
h2a_admin_curl DELETE "admin/v1/server-data"
echo "done !"

# Now, configure possible globals:
h2a_admin_curl POST admin/v1/global-variable 201 ${H2AGENT_GLOBAL_VARIABLE} || exit 1

# Launcher type
read_value "Launcher type" ST_LAUNCHER "h2load|hermes|h2client"

if [ "${ST_LAUNCHER}" = "h2load" ] ################################################### H2LOAD
then
  which h2load &>/dev/null || { echo "Required 'h2load' tool (https://nghttp2.org/documentation/h2load-howto.html)" ; exit 1 ; }

  # Input variables
  read_value "Number of h2load iterations" H2LOAD__ITERATIONS
  read_value "Number of h2load clients" H2LOAD__CLIENTS
  H2LOAD__THREADS__dflt=${H2LOAD__CLIENTS} # $(nproc --all)
  read_value "Number of h2load threads" H2LOAD__THREADS
  read_value "Number of h2load concurrent streams" H2LOAD__CONCURRENT_STREAMS

  # Build request
  s_DATA_OPT=
  case ${ST_REQUEST_METHOD} in
    POST) echo ${ST_REQUEST_BODY} > ${TMP_DIR}/request.json ; s_DATA_OPT="-d ${TMP_DIR}/request.json" ;;
    GET) ;;
    *) echo "ERROR: only POST|GET are supported by h2load" ; exit 1 ;;
  esac
  echo ${ST_REQUEST_BODY} > ${TMP_DIR}/request.json

  REPORT__ref=./report_delay${H2AGENT__RESPONSE_DELAY_MS}_iters${H2LOAD__ITERATIONS}_c${H2LOAD__CLIENTS}_t${H2LOAD__THREADS}_m${H2LOAD__CONCURRENT_STREAMS}.txt
  init_report H2LOAD__ITERATIONS H2LOAD__CLIENTS H2LOAD__THREADS H2LOAD__CONCURRENT_STREAMS

  echo
  echo
  set -x
  time h2load ${H2LOAD__EXTRA_ARGS} -t${H2LOAD__THREADS} -n${H2LOAD__ITERATIONS} -c${H2LOAD__CLIENTS} -m${H2LOAD__CONCURRENT_STREAMS} http://${H2AGENT__BIND_ADDRESS}:${H2AGENT__TRAFFIC_PORT}/${ST_REQUEST_URL} ${s_DATA_OPT} | tee -a ${REPORT}
  set +x

elif [ "${ST_LAUNCHER}" = "hermes" ] ################################################# HERMES
then
  # Input variables
  read_value "Requests per second" HERMES__RPS
  read_value "Test duration (seconds)" HERMES__DURATION
  read_value "Response code expected" HERMES__EXPECTED_RESPONSE_CODE

  # Build traffic profile
  mkdir ${TMP_DIR}/scripts
  cat << EOF > ${TMP_DIR}/scripts/traffic.json
{
  "dns": "${H2AGENT__BIND_ADDRESS}",
  "port": "${H2AGENT__TRAFFIC_PORT}",
  "timeout": 20000,
  "flow": [
    "Request1"
  ],
  "messages": {
    "Request1": {
      "method": "${ST_REQUEST_METHOD}",
      "url": "${ST_REQUEST_URL}",
      "response": {
        "code": ${HERMES__EXPECTED_RESPONSE_CODE}
      },
      "body": ${ST_REQUEST_BODY}
    }
  }
}
EOF

  REPORT__ref=./report_delay${H2AGENT__RESPONSE_DELAY_MS}_rps${HERMES__RPS}_t${HERMES__DURATION}.txt
  init_report HERMES__RPS HERMES__DURATION HERMES__EXPECTED_RESPONSE_CODE

  echo
  echo
  echo "To interrupt, execute:"
  echo "   sudo kill -SIGKILL \$(pgrep hermes)"
  echo
  set -x
  time docker run -it --network host -v ${TMP_DIR}/scripts:/etc/scripts --entrypoint /hermes/hermes ${HERMES_IMG} -r${HERMES__RPS} -p1 -t${HERMES__DURATION} | tee -a ${REPORT}
  set +x

elif [ "${ST_LAUNCHER}" = "h2client" ] ################################################# H2CLIENT
then
  which docker &>/dev/null || { echo "Required 'docker'" ; exit 1 ; }

  read_value "Client h2agent admin port" H2CLIENT__ADMIN_PORT
  read_value "Client h2agent prometheus port" H2CLIENT__PROMETHEUS_PORT
  read_value "Requests per second" H2CLIENT__RPS
  read_value "Number of iterations" H2CLIENT__ITERATIONS

  # Start client-only h2agent (same image as run.sh, detached)
  H2AGENT_DCK_IMG=${H2AGENT_DCK_IMG:-ghcr.io/testillano/h2agent}
  H2AGENT_DCK_TAG=${H2AGENT_DCK_TAG:-latest}
  H2AGENT_DCK_EXTRA_ARGS=${H2AGENT_DCK_EXTRA_ARGS:-"--network=host"}
  if [ -z "${H2AGENT_LD_PRELOAD+x}" ]; then
    H2AGENT_LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.2
  fi
  ld_preload_arg=
  [ -n "${H2AGENT_LD_PRELOAD}" ] && ld_preload_arg="-e LD_PRELOAD=${H2AGENT_LD_PRELOAD}"

  # Stop leftover client container if exists
  docker kill h2agent-client 2>/dev/null
  docker rm h2agent-client 2>/dev/null

  # Check ports are free
  for port in ${H2CLIENT__ADMIN_PORT} ${H2CLIENT__PROMETHEUS_PORT}; do
    ss -tln | grep -q ":${port} " && echo "ERROR: port ${port} already in use (run: ss -tlnp | grep ${port})" && exit 1
  done

  H2CLIENT_CONTAINER=$(docker run -d --rm --name h2agent-client ${H2AGENT_DCK_EXTRA_ARGS} ${ld_preload_arg} \
    ${H2AGENT_DCK_IMG}:${H2AGENT_DCK_TAG} \
    --admin-port ${H2CLIENT__ADMIN_PORT} --prometheus-port ${H2CLIENT__PROMETHEUS_PORT} --traffic-server-port -1 --log-level Warning)

  [ -z "${H2CLIENT_CONTAINER}" ] && echo "ERROR: failed to start client h2agent container (image: ${H2AGENT_DCK_IMG}:${H2AGENT_DCK_TAG})" && exit 1
  echo "Started client container: ${H2CLIENT_CONTAINER}"
  trap "docker stop ${H2CLIENT_CONTAINER} 2>/dev/null; rm -rf ${TMP_DIR}" EXIT

  # Wait for client h2agent to be ready
  echo -n "Waiting for client h2agent to start..."
  for i in $(seq 1 30); do
    sleep 1
    curl -sf --http2-prior-knowledge "http://localhost:${H2CLIENT__ADMIN_PORT}/admin/v1/logging" &>/dev/null && break
    echo -n "."
  done
  curl -sf --http2-prior-knowledge "http://localhost:${H2CLIENT__ADMIN_PORT}/admin/v1/logging" &>/dev/null \
    || { echo -e "\nERROR: client h2agent did not start (check docker logs h2agent-client)" ; exit 1 ; }
  echo " ready"

  # $1: method; $2: uri; $3: optional body file
  h2a_client_curl() {
    local s_data=
    [ -n "$3" ] && s_data="-d@$3"
    curl -s --http2-prior-knowledge -H 'content-type: application/json' -X$1 ${s_data} \
      "http://localhost:${H2CLIENT__ADMIN_PORT}/$2"
  }

  # Disable client data storage (bottleneck at high rps) - use timing instead
  h2a_client_curl PUT "admin/v1/client-data/configuration?discard=true&discardKeyHistory=true&disablePurge=true" > /dev/null

  # Configure client endpoint pointing to server h2agent
  echo "{\"id\":\"server\",\"host\":\"${H2AGENT__BIND_ADDRESS}\",\"port\":${H2AGENT__TRAFFIC_PORT}}" > ${TMP_DIR}/client-endpoint.json
  h2a_client_curl POST admin/v1/client-endpoint ${TMP_DIR}/client-endpoint.json

  # Configure client provision
  PROVISION_JSON="{\"id\":\"benchmark\",\"endpoint\":\"server\",\"requestMethod\":\"${ST_REQUEST_METHOD}\",\"requestUri\":\"/${ST_REQUEST_URL}\""
  [ "${ST_REQUEST_METHOD}" = "POST" ] && PROVISION_JSON="${PROVISION_JSON},\"requestBody\":${ST_REQUEST_BODY}"
  PROVISION_JSON="${PROVISION_JSON}}"
  echo "${PROVISION_JSON}" > ${TMP_DIR}/client-provision.json
  h2a_client_curl POST admin/v1/client-provision ${TMP_DIR}/client-provision.json

  REPORT__ref=./report_delay${H2AGENT__RESPONSE_DELAY_MS}_rps${H2CLIENT__RPS}_iters${H2CLIENT__ITERATIONS}.txt
  init_report H2CLIENT__ADMIN_PORT H2CLIENT__PROMETHEUS_PORT H2CLIENT__RPS H2CLIENT__ITERATIONS

  # Trigger timer-based load
  EXPECTED_SECS=$(( (H2CLIENT__ITERATIONS + H2CLIENT__RPS - 1) / H2CLIENT__RPS ))
  echo
  echo "Triggering ${H2CLIENT__ITERATIONS} requests at ${H2CLIENT__RPS} rps (~${EXPECTED_SECS}s expected)..."
  START_NS=$(date +%s%N)
  h2a_client_curl GET "admin/v1/client-provision/benchmark?sequenceBegin=1&sequenceEnd=${H2CLIENT__ITERATIONS}&rps=${H2CLIENT__RPS}"
  echo

  # Wait for timer to complete by polling dynamics.sequence
  TIMEOUT_SECS=$(( (H2CLIENT__ITERATIONS / H2CLIENT__RPS + 1) * 3 ))
  echo "Waiting for completion (timeout ${TIMEOUT_SECS}s)..."
  while true; do
    sleep 0.2
    seq=$(h2a_client_curl GET "admin/v1/client-provision" 2>/dev/null | \
      python3 -c "import sys,json; d=json.load(sys.stdin); p=[x for x in d if x.get('id')=='benchmark']; print(p[0]['dynamics']['sequence'] if p else 0)" 2>/dev/null || echo 0)
    [ "${seq:-0}" -gt "${H2CLIENT__ITERATIONS}" ] && break
    now=$(date +%s%N)
    [ $(( (now - START_NS) / 1000000000 )) -ge ${TIMEOUT_SECS} ] && echo "Timeout!" && break
  done
  END_NS=$(date +%s%N)

  ELAPSED_MS=$(( (END_NS - START_NS) / 1000000 ))
  ACTUAL_RPS=$(( H2CLIENT__ITERATIONS * 1000 / ELAPSED_MS ))
  echo
  echo "Completed: ${H2CLIENT__ITERATIONS} requests in ${ELAPSED_MS}ms (~${ACTUAL_RPS} req/s)" | tee -a ${REPORT}
fi

rm -f last
ln -s ${REPORT} last
echo
echo "Created test report:"
echo "  last -> ${REPORT}"

