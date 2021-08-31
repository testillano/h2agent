#!/bin/bash
#
#############
# VARIABLES #
#############
H2AGENT__RESPONSE_DELAY_MS__dflt=0
HERMES__RPS__dflt=5000
HERMES__DURATION__dflt=10

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

cd $(dirname $0)

read_value "h2agent response delay in milliseconds" H2AGENT__RESPONSE_DELAY_MS

sed 's/@{H2AGENT__RESPONSE_DELAY_MS}/'${H2AGENT__RESPONSE_DELAY_MS}'/' provision.json.in > provision.json
trap "rm -f provision.json curl.output" EXIT
status=$(curl -s -o curl.output -w "%{http_code}" --http2-prior-knowledge -XPOST -d@provision.json -H 'content-type: application/json' http://localhost:8074/admin/v1/server-provision)
if [ "${status}" != "201" ]
then
  cat curl.output 2>/dev/null
  [ $? -ne 0 ] && echo "ERROR: check if 'h2agent' is started"
  exit 1
fi

read_value "requests per second" HERMES__RPS
read_value "test duration (seconds)" HERMES__DURATION

curl -s -o /dev/null --http2-prior-knowledge -XPUT "http://localhost:8074/admin/v1/server-data/configuration?discard=${DISCARD_SERVER_DATA}&discardRequestsHistory=${DISCARD_SERVER_DATA_HISTORY}"
curl -s -o /dev/null --http2-prior-knowledge -XDELETE "http://localhost:8074/admin/v1/server-data"
echo -e "\nServer data configuration:"
curl --http2-prior-knowledge -XGET "http://localhost:8074/admin/v1/server-data/configuration"

REPORT__ref=./report_delay${H2AGENT__RESPONSE_DELAY_MS}_rps${HERMES__RPS}_t${HERMES__DURATION}.txt
REPORT=${REPORT__ref}
seq=1 ; while [ -f ${REPORT} ]; do REPORT=${REPORT__ref}.${seq}; seq=$((seq+1)); done
for var in H2AGENT__RESPONSE_DELAY_MS HERMES__RPS HERMES__DURATION
do
  echo -n "${var}=$(eval echo \$$var) "
done > ${REPORT}
echo -e "${PWD}/start.sh\n\n\n" >> ${REPORT}
echo
echo
echo "To interrupt, execute:"
echo "   sudo kill -SIGKILL \$(pgrep hermes)"
echo
set -x
time docker run -it --network host -v ${PWD}/script:/etc/scripts --entrypoint /hermes/hermes jgomezselles/hermes:0.0.1 -r${HERMES__RPS} -p1 -t${HERMES__DURATION} | tee -a ${REPORT}
set +x

rm -f last
ln -s ${REPORT} last
echo
echo "Created test report:"
echo "  last -> ${REPORT}"

