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

wait_for_event() {
  local id=$1 timeout=10
  local start=$(date +%s)
  while true; do
    sleep 0.3
    local count
    count=$(curl -sf --http2-prior-knowledge \
      "http://${H2AGENT_CLIENT_ADMIN_ENDPOINT}/admin/v1/client-data" 2>/dev/null | \
      jq --arg id "$id" '[.[] | .events[] | select(.clientProvisionId==$id)] | length' 2>/dev/null || echo 0)
    [ "${count:-0}" -ge 1 ] && return 0
    [ $(( $(date +%s) - start )) -ge $timeout ] && echo "Timeout waiting for event $id!" && return 1
  done
}

# Trigger store-item (POST /items/id-42)
h2a_curl GET "client-provision/store-item?sequenceBegin=1&sequenceEnd=1&rps=1" >/dev/null
wait_for_event store-item || exit 1

# Trigger read-item (GET /items/id-42)
h2a_curl GET "client-provision/read-item?sequenceBegin=1&sequenceEnd=1&rps=1" >/dev/null
wait_for_event read-item || exit 1

client_data=$(curl -sf --http2-prior-knowledge \
  "http://${H2AGENT_CLIENT_ADMIN_ENDPOINT}/admin/v1/client-data")

# Step 1: POST returned 201
title "Step 1: store-item (POST /items/id-42) returned 201" "" "*"
status=$(echo "${client_data}" | jq '[.[] | .events[] | select(.clientProvisionId=="store-item")] | .[-1].responseStatusCode // 0')
if [ "${status}" -eq 201 ]; then
  echo -e "${COLOR_green}OK: POST returned 201${COLOR_reset}"
else
  echo -e "${COLOR_red}NOK: expected 201, got ${status}${COLOR_reset}"
  exit 1
fi

# Step 2: GET returned 200 with stored body
title "Step 2: read-item (GET /items/id-42) returned 200 with stored body" "" "*"
status=$(echo "${client_data}" | jq '[.[] | .events[] | select(.clientProvisionId=="read-item")] | .[-1].responseStatusCode // 0')
body=$(echo "${client_data}" | jq -c '[.[] | .events[] | select(.clientProvisionId=="read-item")] | .[-1].responseBody | if type == "string" then fromjson else . end // {}')
if [ "${status}" -eq 200 ] && [ "${body}" = '{"value":42}' ]; then
  echo -e "${COLOR_green}OK: GET returned 200 with body ${body}${COLOR_reset}"
else
  echo -e "${COLOR_red}NOK: expected 200 + {\"value\":42}, got ${status} + ${body}${COLOR_reset}"
  exit 1
fi

# Step 3: second GET returns 404 (item forgotten)
title "Step 3: second GET /items/id-42 returns 404 (item forgotten)" "" "*"
EXPECTED_STATUS_CODES=404
test_query "Second GET (item forgotten)" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/items/id-42" || exit 1

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff master...kata-solutions -- client-provision.json server-provision.json
