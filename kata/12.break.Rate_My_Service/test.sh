#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.bash

H2AGENT_CLIENT_ADMIN_ENDPOINT=localhost:8074

h2a_curl() {
  curl -sf --http2-prior-knowledge -H 'content-type: application/json' -X$1 ${3:+-d@$3} \
    "http://${H2AGENT_CLIENT_ADMIN_ENDPOINT}/admin/v1/$2"
}

h2agent_server_configuration

# Configure client
h2a_curl POST client-endpoint client-endpoint.json >/dev/null
h2a_curl POST client-provision client-provision.json >/dev/null

ITERATIONS=50
CPS=5
EXPECTED_SECS=$(( ITERATIONS / CPS ))
TIMEOUT=$(( EXPECTED_SECS * 3 + 5 ))

title "Triggering ${ITERATIONS} provisions at ${CPS} cps (~${EXPECTED_SECS}s expected)" "" "*"
START_NS=$(date +%s%N)
h2a_curl GET "client-provision/rater?sequenceBegin=1&sequenceEnd=${ITERATIONS}&cps=${CPS}" >/dev/null

echo "Waiting for completion (timeout ${TIMEOUT}s)..."
while true; do
  sleep 0.5
  seq=$(h2a_curl GET "client-provision" 2>/dev/null | \
    jq '[.[] | select(.id=="rater")] | .[0].dynamics.sequence // 0' 2>/dev/null || echo 0)
  [ "${seq:-0}" -ge "${ITERATIONS}" ] && break
  [ $(( ($(date +%s%N) - START_NS) / 1000000000 )) -ge ${TIMEOUT} ] && echo "Timeout!" && break
done
END_NS=$(date +%s%N)

ELAPSED_MS=$(( (END_NS - START_NS) / 1000000 ))
ACTUAL_CPS=$(( ITERATIONS * 1000 / ELAPSED_MS ))
echo "Elapsed: ${ELAPSED_MS}ms, actual cps: ${ACTUAL_CPS}"

# Verify within ±20% of target cps
MIN_CPS=$(( CPS * 80 / 100 ))
MAX_CPS=$(( CPS * 120 / 100 ))
if [ "${ACTUAL_CPS}" -ge "${MIN_CPS}" ] && [ "${ACTUAL_CPS}" -le "${MAX_CPS}" ]; then
  echo -e "${COLOR_green}OK: ${ACTUAL_CPS} cps is within ±20% of target ${CPS} cps${COLOR_reset}"
else
  echo -e "${COLOR_red}NOK: ${ACTUAL_CPS} cps is outside ±20% of target ${CPS} cps (expected ${MIN_CPS}-${MAX_CPS})${COLOR_reset}"
  exit 1
fi

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff -- client-endpoint.json client-provision.json
