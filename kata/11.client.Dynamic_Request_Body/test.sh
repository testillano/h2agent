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

# Cleanup client state
curl -sf --http2-prior-knowledge -XDELETE \
  "http://${H2AGENT_CLIENT_ADMIN_ENDPOINT}/admin/v1/client-provision" >/dev/null
curl -sf --http2-prior-knowledge -XDELETE \
  "http://${H2AGENT_CLIENT_ADMIN_ENDPOINT}/admin/v1/client-endpoint" >/dev/null

# Configure client
h2a_curl POST client-endpoint client-endpoint.json >/dev/null
h2a_curl POST client-provision client-provision.json >/dev/null

# Trigger 5 bounces at 2 cps
BOUNCES=5
h2a_curl GET "client-provision/ping?sequenceBegin=1&sequenceEnd=${BOUNCES}&cps=2" >/dev/null

# Wait for completion
echo "Waiting for ${BOUNCES} bounces..."
TIMEOUT=15
START=$(date +%s)
while true; do
  sleep 0.5
  seq=$(h2a_curl GET "client-provision" 2>/dev/null | \
    jq '[.[] | select(.id=="ping")] | .[0].dynamics.sequence // 0' 2>/dev/null || echo 0)
  [ "${seq:-0}" -ge "${BOUNCES}" ] && break
  [ $(( $(date +%s) - START )) -ge ${TIMEOUT} ] && echo "Timeout!" && break
done

# Verify the last request body had bounce=BOUNCES (requires student to add sequence transform)
title "Check last bounce had bounce=${BOUNCES} in request body" "" "*"

last_bounce=$(curl -sf --http2-prior-knowledge \
  "http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-data" | \
  jq '[.[] | select(.uri=="/game/ping" and .method=="POST")] | .[0].events | .[-1].requestBody.bounce // 0' \
  2>/dev/null || echo 0)

echo "Last bounce value received by server: ${last_bounce}"
if [ "${last_bounce:-0}" -eq "${BOUNCES}" ]; then
  echo -e "${COLOR_green}OK: last bounce = ${BOUNCES}${COLOR_reset}"
else
  echo -e "${COLOR_red}NOK: expected bounce=${BOUNCES}, got ${last_bounce:-0}${COLOR_reset}"
  exit 1
fi

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff -- client-endpoint.json client-provision.json
