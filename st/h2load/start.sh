#!/bin/bash
#
# Some examples to benchmark:
#
# 1) Test that h2agent delay timers are managed asynchronously for the worker thread, freeing the
#    tatsuhiro's nghttp2 io service for an specific stream:
#
#    H2AGENT__RESPONSE_DELAY_MS=1000 H2LOAD__ITERATIONS=100 H2LOAD__CLIENTS=1 H2LOAD__THREADS=1 H2LOAD__CONCURRENT_STREAMS=100 ./start.sh
#
#    As m=100, all the iterations (100) must be managed in about 1 second:
#
#    finished in 1.01s, 98.70 req/s, 105.88KB/s
#    requests: 100 total, 100 started, 100 done, 100 succeeded, 0 failed, 0 errored, 0 timeout
#    status codes: 100 2xx, 0 3xx, 0 4xx, 0 5xx
#    traffic: 107.28KB (109859) total, 335B (335) headers (space savings 95.28%), 105.18KB (107700) data
#                      min         max         mean         sd        +/- sd
#    time for request:      1.01s       1.01s       1.01s      1.96ms    41.00%
#    time for connect:      255us       255us       255us         0us   100.00%
#    time to 1st byte:      1.01s       1.01s       1.01s         0us   100.00%
#    req/s           :      98.75       98.75       98.75        0.00   100.00%
#
#    Older h2agent versions took 100 seconds as the delay was processed whithin the stream context blocking the server io service.
#
# 2) Test that no memory leaks arise for the asyncronous timers:
#
#    H2AGENT__RESPONSE_DELAY_MS=1 H2LOAD__ITERATIONS=1000000 H2LOAD__CLIENTS=1 H2LOAD__THREADS=1 H2LOAD__CONCURRENT_STREAMS=100 ./start.sh
#
#    %MEM should be stable at 0% (use for example: 'top -H -p $(pgrep h2agent)')
#
# 3) Test high load, to get about 14k req/s in 8-cpu machine with default startup and these parameters:
#
#    H2AGENT__RESPONSE_DELAY_MS=0 H2LOAD__ITERATIONS=100000 H2LOAD__CLIENTS=1 H2LOAD__THREADS=1 H2LOAD__CONCURRENT_STREAMS=100 ./start.sh
#
#    finished in 7.26s, 13779.98 req/s, 14.43MB/s
#    requests: 100000 total, 100000 started, 100000 done, 100000 succeeded, 0 failed, 0 errored, 0 timeout
#    status codes: 100000 2xx, 0 3xx, 0 4xx, 0 5xx
#    traffic: 104.72MB (109809330) total, 293.18KB (300219) headers (space savings 95.77%), 102.71MB (107700000) data
#                         min         max         mean         sd        +/- sd
#    time for request:     1.89ms     21.95ms      7.20ms      1.35ms    82.44%
#    time for connect:      196us       196us       196us         0us   100.00%
#    time to 1st byte:    10.30ms     10.30ms     10.30ms         0us   100.00%
#    req/s           :   13780.40    13780.40    13780.40        0.00   100.00%

#############
# VARIABLES #
#############
H2AGENT__ENDPOINT__dflt=localhost
H2AGENT__RESPONSE_DELAY_MS__dflt=0
H2LOAD__ITERATIONS__dflt=100000
H2LOAD__CLIENTS__dflt=1
H2LOAD__CONCURRENT_STREAMS__dflt=100

DISCARD_SERVER_DATA=${DISCARD_SERVER_DATA:-true}
DISCARD_SERVER_DATA_HISTORY=${DISCARD_SERVER_DATA_HISTORY:-true}

#############
# FUNCTIONS #
#############
# $1: what; $2: output
read_value() {
  local what=$1
  local -n output=$2

  local default=$(eval echo \$$2__dflt)
  echo
  [ -n "${output}" ] && echo "Prepended ${what}: ${output}" && return 0
  echo -en "Input ${what}\n (or set '$2' to be non-interactive) [${default}]: "
  read output
  [ -z "${output}" ] && output=${default} && echo ${output}
}

#############
# EXECUTION #
#############
echo
which h2load &>/dev/null || { echo "Required 'h2load' tool (https://nghttp2.org/documentation/h2load-howto.html)" ; exit 1 ; }

cd $(dirname $0)

read_value "h2agent endpoint address" H2AGENT__ENDPOINT

read_value "h2agent response delay in milliseconds" H2AGENT__RESPONSE_DELAY_MS
sed 's/@{H2AGENT__RESPONSE_DELAY_MS}/'${H2AGENT__RESPONSE_DELAY_MS}'/' provision.json.in > provision.json
trap "rm -f provision.json curl.output" EXIT
status=$(curl -s -o curl.output -w "%{http_code}" --http2-prior-knowledge -XPOST -d@provision.json -H 'content-type: application/json' http://${H2AGENT__ENDPOINT}:8074/admin/v1/server-provision)
if [ "${status}" != "201" ]
then
  cat curl.output 2>/dev/null
  [ $? -ne 0 ] && echo "ERROR: check if 'h2agent' is started"
  exit 1
fi
# Duration correction
[ ${H2AGENT__RESPONSE_DELAY_MS} -ne 0 ] && H2LOAD__ITERATIONS__dflt=$((H2LOAD__ITERATIONS__dflt/H2AGENT__RESPONSE_DELAY_MS))

read_value "number of h2load iterations" H2LOAD__ITERATIONS
read_value "number of h2load clients" H2LOAD__CLIENTS
H2LOAD__THREADS__dflt=${H2LOAD__CLIENTS} # $(nproc --all)
read_value "number of h2load threads" H2LOAD__THREADS
read_value "number of h2load concurrent streams" H2LOAD__CONCURRENT_STREAMS

curl -s -o /dev/null --http2-prior-knowledge -XPUT "http://${H2AGENT__ENDPOINT}:8074/admin/v1/server-data/configuration?discard=${DISCARD_SERVER_DATA}&discardRequestsHistory=${DISCARD_SERVER_DATA_HISTORY}"
curl -s -o /dev/null --http2-prior-knowledge -XDELETE "http://${H2AGENT__ENDPOINT}:8074/admin/v1/server-data"
echo -e "\nServer data configuration:"
curl --http2-prior-knowledge -XGET "http://${H2AGENT__ENDPOINT}:8074/admin/v1/server-data/configuration"

REPORT__ref=./report_delay${H2AGENT__RESPONSE_DELAY_MS}_iters${H2LOAD__ITERATIONS}_c${H2LOAD__CLIENTS}_t${H2LOAD__THREADS}_m${H2LOAD__CONCURRENT_STREAMS}.txt
REPORT=${REPORT__ref}
seq=1 ; while [ -f ${REPORT} ]; do REPORT=${REPORT__ref}.${seq}; seq=$((seq+1)); done
for var in H2AGENT__RESPONSE_DELAY_MS H2LOAD__ITERATIONS H2LOAD__CLIENTS H2LOAD__THREADS H2LOAD__CONCURRENT_STREAMS
do
  echo -n "${var}=$(eval echo \$$var) "
done > ${REPORT}
echo -e "${PWD}/start.sh\n\n\n" >> ${REPORT}

echo
echo
set -x
time h2load -t${H2LOAD__THREADS} -n${H2LOAD__ITERATIONS} -c${H2LOAD__CLIENTS} -m${H2LOAD__CONCURRENT_STREAMS} http://${H2AGENT__ENDPOINT}:8000/load-test/v1/id-21 -d ./request.json | tee -a ${REPORT}
set +x

rm -f last
ln -s ${REPORT} last
echo
echo "Created test report:"
echo "  last -> ${REPORT}"

