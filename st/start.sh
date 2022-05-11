#!/bin/bash

#############
# VARIABLES #
#############
DEFAULTS=
[ "$1" = "-y" ] && DEFAULTS=true

# Default values
H2AGENT_VALIDATE_SCHEMAS__dflt=n
H2AGENT_SCHEMA__dflt=schema.json
H2AGENT_MATCHING__dflt=matching.json
H2AGENT_PROVISION__dflt=provision.json
H2AGENT_GLOBALS__dflt=globals.json
H2AGENT__SERVER_DATA_STORAGE_CONFIGURATION__dflt=discard-all
H2AGENT__SERVER_DATA_PURGE_CONFIGURATION__dflt=disable-purge
H2AGENT__ENDPOINT__dflt=0.0.0.0
H2AGENT__RESPONSE_DELAY_MS__dflt=0

ST_REQUEST_METHOD__dflt="POST"
ST_REQUEST_URL__dflt="/load-test/v1/id-21"
ST_LAUNCHER__dflt=h2load

H2LOAD__ITERATIONS__dflt=100000
H2LOAD__CLIENTS__dflt=1
#H2LOAD__THREADS__dflt=1
H2LOAD__CONCURRENT_STREAMS__dflt=100

HERMES__RPS__dflt=5000
HERMES__DURATION__dflt=10
HERMES__EXPECTED_RESPONSE_CODE__dflt=200

# Fixed values
ST_REQUEST_BODY='{ "id": "1a8b8863", "name": "Ada Lovelace", "email": "ada@geemail.com", "bio": "First programmer. No big deal.", "age": 198, "avatar": "http://en.wikipedia.org/wiki/File:Ada_lovelace.jpg" }'
H2AGENT__ADMIN_PORT=8074
H2AGENT__TRAFFIC_PORT=8000

# Common variables
COMMON_VARS="H2AGENT_VALIDATE_SCHEMAS H2AGENT_SCHEMA H2AGENT_MATCHING H2AGENT_PROVISION H2AGENT_GLOBALS H2AGENT__SERVER_DATA_STORAGE_CONFIGURATION H2AGENT__SERVER_DATA_PURGE_CONFIGURATION H2AGENT__ENDPOINT H2AGENT__RESPONSE_DELAY_MS ST_REQUEST_METHOD ST_REQUEST_URL ST_LAUNCHER"

#############
# FUNCTIONS #
#############
usage() {

  cat << EOF
Usage: $0 [-h|--help] [-y]

          -h|--help: this help.
          -y:        assume defaults for all the questions.


 Some examples to benchmark:

 1) Test that h2agent delay timers are managed asynchronously for the worker thread, freeing the
    tatsuhiro's nghttp2 io service for an specific stream:

    H2AGENT__RESPONSE_DELAY_MS=1000 H2LOAD__ITERATIONS=100 ./start.sh -y

    As m=100, all the iterations (100) must be managed in about 1 second:

    finished in 1.01s, 98.70 req/s, 105.88KB/s
    requests: 100 total, 100 started, 100 done, 100 succeeded, 0 failed, 0 errored, 0 timeout
    status codes: 100 2xx, 0 3xx, 0 4xx, 0 5xx
    traffic: 107.28KB (109859) total, 335B (335) headers (space savings 95.28%), 105.18KB (107700) data
                      min         max         mean         sd        +/- sd
    time for request:      1.01s       1.01s       1.01s      1.96ms    41.00%
    time for connect:      255us       255us       255us         0us   100.00%
    time to 1st byte:      1.01s       1.01s       1.01s         0us   100.00%
    req/s           :      98.75       98.75       98.75        0.00   100.00%

    Older h2agent versions took 100 seconds as the delay was processed whithin the stream context blocking the server io service.

 2) Test that no memory leaks arise for the asyncronous timers:

    H2AGENT__RESPONSE_DELAY_MS=1 H2LOAD__ITERATIONS=1000000 ./start.sh -y

    %MEM should be stable at 0% (use for example: 'top -H -p $(pgrep h2agent)')

 3) Test high load, to get about 14k req/s in 8-cpu machine with default startup and these parameters:

    H2LOAD__ITERATIONS=100000 ./start.sh -y

    finished in 7.26s, 13779.98 req/s, 14.43MB/s
    requests: 100000 total, 100000 started, 100000 done, 100000 succeeded, 0 failed, 0 errored, 0 timeout
    status codes: 100000 2xx, 0 3xx, 0 4xx, 0 5xx
    traffic: 104.72MB (109809330) total, 293.18KB (300219) headers (space savings 95.77%), 102.71MB (107700000) data
                         min         max         mean         sd        +/- sd
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

  status=$(curl -s -w "%{http_code}" --http2-prior-knowledge -o ${TMP_DIR}/curl.output -H 'content-type: application/json' -X${method} ${s_dataFile_option} "http://${H2AGENT__ENDPOINT}:${H2AGENT__ADMIN_PORT}/${uri}")

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
  [ -n "${output}" ] && echo "Prepended ${what}: ${output}" && check_valid_values "${output}" "${validValues}" && return $?
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
    echo "${var}=\"$(eval echo \$$var)\" \\"
  done >> ${REPORT}
  echo -e "${PWD}/start.sh" >> ${REPORT}
  echo -e "----------------------------------------------------\n" >> ${REPORT}
}

#############
# EXECUTION #
#############
echo
cd $(dirname $0)
[ "$1" = "-h" -o "$1" = "--help" ] && usage && exit 0

# Requirements
which jq &>/dev/null || { echo "Required 'jq' tool (https://stedolan.github.io/jq/)" ; exit 1 ; }

read_value "Validate schemas" H2AGENT_VALIDATE_SCHEMAS "y|n"
if [ "${H2AGENT_VALIDATE_SCHEMAS}" = "y" ]
then
  read_value "Schema configuration" H2AGENT_SCHEMA
  [ ! -f "${H2AGENT_SCHEMA}" ] && echo "ERROR: missing file '${H2AGENT_SCHEMA}' !" && exit 1
fi
read_value "Matching configuration" H2AGENT_MATCHING
[ ! -f "${H2AGENT_MATCHING}" ] && echo "ERROR: missing file '${H2AGENT_MATCHING}' !" && exit 1
read_value "Provision configuration" H2AGENT_PROVISION
[ ! -f "${H2AGENT_PROVISION}" ] &&  echo "ERROR: missing file '${H2AGENT_PROVISION}' !" && exit 1
read_value "Global variables configuration" H2AGENT_GLOBALS
[ ! -f "${H2AGENT_GLOBALS}" ] &&  echo "ERROR: missing file '${H2AGENT_GLOBALS}' !" && exit 1
read_value "Server data storage configuration" H2AGENT__SERVER_DATA_STORAGE_CONFIGURATION "discard-all|discard-history|keep-all" || exit 1
read_value "Server data purge configuration" H2AGENT__SERVER_DATA_PURGE_CONFIGURATION "enable-purge|disable-purge" || exit 1
read_value "H2agent endpoint address" H2AGENT__ENDPOINT
read_value "H2agent response delay in milliseconds" H2AGENT__RESPONSE_DELAY_MS
[ ${H2AGENT__RESPONSE_DELAY_MS} -ne 0 ] && H2LOAD__ITERATIONS__dflt=$((H2LOAD__ITERATIONS__dflt/H2AGENT__RESPONSE_DELAY_MS)) # duration correction

read_value "Request method" ST_REQUEST_METHOD "PUT|DELETE|HEAD|POST|GET" || exit 1

read_value "Request url" ST_REQUEST_URL
ST_REQUEST_URL=$(echo ${ST_REQUEST_URL} | sed -e 's/^\///') # normalize to not have leading slash

TMP_DIR=$(mktemp -d)
trap "rm -rf ${TMP_DIR}" EXIT

# build provision:
jq --arg replace "${H2AGENT__RESPONSE_DELAY_MS}" '. |= map(if .responseDelayMs == 0 then (.responseDelayMs=($replace | tonumber)) else . end)' ${H2AGENT_PROVISION} | \
jq --arg replace "${ST_REQUEST_METHOD}" '. |= map(if .requestMethod == "POST" then (.requestMethod=($replace)) else . end)' | \
jq --arg replace "/${ST_REQUEST_URL}" '. |= map(if .requestUri == "URI" then (.requestUri=($replace)) else . end)' > ${TMP_DIR}/provision.json
if [ "${H2AGENT_VALIDATE_SCHEMAS}" != "y" ]
then
  jq 'del (.[0].requestSchemaId,.[0].responseSchemaId)' ${TMP_DIR}/provision.json > ${TMP_DIR}/provision.json2
  mv ${TMP_DIR}/provision.json2 ${TMP_DIR}/provision.json
fi

[ "${H2AGENT_VALIDATE_SCHEMAS}" = "y" ] && { h2a_admin_curl POST admin/v1/schema 201 ${H2AGENT_SCHEMA} || exit 1 ; }
h2a_admin_curl POST admin/v1/server-matching 201 ${H2AGENT_MATCHING} || exit 1
h2a_admin_curl POST admin/v1/server-provision 201 ${TMP_DIR}/provision.json || exit 1

# Server data configuration
case ${H2AGENT__SERVER_DATA_STORAGE_CONFIGURATION} in
  discard-all) DISCARD_SERVER_DATA=true; DISCARD_SERVER_DATA_HISTORY=true ;;
  discard-history) DISCARD_SERVER_DATA=false; DISCARD_SERVER_DATA_HISTORY=true ;;
  keep-all) DISCARD_SERVER_DATA=false; DISCARD_SERVER_DATA_HISTORY=false ;;
esac
case ${H2AGENT__SERVER_DATA_PURGE_CONFIGURATION} in
  enable-purge) DISABLE_PURGE=false ;;
  disable-purge) DISABLE_PURGE=true ;;
esac
h2a_admin_curl PUT "admin/v1/server-data/configuration?discard=${DISCARD_SERVER_DATA}&discardRequestsHistory=${DISCARD_SERVER_DATA_HISTORY}&disablePurge=${DISABLE_PURGE}" 200 || exit 1
echo -e "\nServer data configuration:"
h2a_admin_curl GET "admin/v1/server-data/configuration" || exit 1
cat ${TMP_DIR}/curl.output
echo -en "\n\nRemoving current server data information ... "
h2a_admin_curl DELETE "admin/v1/server-data"
echo "done !"

# Now, configure possible globals:
h2a_admin_curl POST admin/v1/server-data/global 201 ${H2AGENT_GLOBALS} || exit 1

# Launcher type
read_value "Launcher type" ST_LAUNCHER "h2load|hermes"

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
  time h2load -t${H2LOAD__THREADS} -n${H2LOAD__ITERATIONS} -c${H2LOAD__CLIENTS} -m${H2LOAD__CONCURRENT_STREAMS} http://${H2AGENT__ENDPOINT}:${H2AGENT__TRAFFIC_PORT}/${ST_REQUEST_URL} ${s_DATA_OPT} | tee -a ${REPORT}
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
  "dns": "${H2AGENT__ENDPOINT}",
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
  time docker run -it --network host -v ${TMP_DIR}/scripts:/etc/scripts --entrypoint /hermes/hermes jgomezselles/hermes:0.0.1 -r${HERMES__RPS} -p1 -t${HERMES__DURATION} | tee -a ${REPORT}
  set +x
fi

rm -f last
ln -s ${REPORT} last
echo
echo "Created test report:"
echo "  last -> ${REPORT}"

