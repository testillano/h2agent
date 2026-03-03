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
RPS=5
EXPECTED_SECS=$(( ITERATIONS / RPS ))
TIMEOUT=$(( EXPECTED_SECS * 3 + 5 ))

title "Triggering ${ITERATIONS} requests at ${RPS} rps (~${EXPECTED_SECS}s expected)" "" "*"
START_NS=$(date +%s%N)
h2a_curl GET "client-provision/rater?sequenceBegin=1&sequenceEnd=${ITERATIONS}&rps=${RPS}" >/dev/null

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
ACTUAL_RPS=$(( ITERATIONS * 1000 / ELAPSED_MS ))
echo "Elapsed: ${ELAPSED_MS}ms, actual rps: ${ACTUAL_RPS}"

# Verify within ±20% of target rps
MIN_RPS=$(( RPS * 80 / 100 ))
MAX_RPS=$(( RPS * 120 / 100 ))
if [ "${ACTUAL_RPS}" -ge "${MIN_RPS}" ] && [ "${ACTUAL_RPS}" -le "${MAX_RPS}" ]; then
  echo -e "${COLOR_green}OK: ${ACTUAL_RPS} rps is within ±20% of target ${RPS} rps${COLOR_reset}"
else
  echo -e "${COLOR_red}NOK: ${ACTUAL_RPS} rps is outside ±20% of target ${RPS} rps (expected ${MIN_RPS}-${MAX_RPS})${COLOR_reset}"
  exit 1
fi

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff master...kata-solutions -- client-endpoint.json client-provision.json
