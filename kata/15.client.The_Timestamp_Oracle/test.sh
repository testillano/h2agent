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
curl -sf --http2-prior-knowledge -XDELETE \
  "http://${H2AGENT_CLIENT_ADMIN_ENDPOINT}/admin/v1/client-data" >/dev/null
curl -sf --http2-prior-knowledge -XDELETE \
  "http://${H2AGENT_CLIENT_ADMIN_ENDPOINT}/admin/v1/vault" >/dev/null

# Configure client
h2a_curl POST client-endpoint client-endpoint.json >/dev/null
h2a_curl POST client-provision client-provision.json >/dev/null

wait_for_gvar() {
  local name=$1 timeout=10
  local start=$(date +%s)
  while true; do
    sleep 0.3
    local val
    val=$(h2a_curl GET "vault" 2>/dev/null | jq -r --arg n "$name" '.[$n] // empty' 2>/dev/null || echo "")
    [ -n "${val}" ] && return 0
    [ $(( $(date +%s) - start )) -ge $timeout ] && echo "Timeout waiting for vault.${name}!" && return 1
  done
}

# Trigger first-tick
title "Step 1: first-tick (GET /time, store t1)" "" "*"
h2a_curl GET "client-provision/first-tick?sequenceBegin=1&sequenceEnd=1&cps=1" >/dev/null
wait_for_gvar t1 || exit 1

# Trigger second-tick
title "Step 2: second-tick (GET /time, store t2, compute elapsed)" "" "*"
h2a_curl GET "client-provision/second-tick?sequenceBegin=1&sequenceEnd=1&cps=1" >/dev/null
wait_for_gvar elapsed || exit 1

# Read vault.elapsed
elapsed=$(h2a_curl GET "vault" 2>/dev/null | jq -r '.elapsed // empty' 2>/dev/null || echo "")
echo "vault.elapsed = ${elapsed}"

if [ -z "${elapsed}" ]; then
  echo -e "${COLOR_red}NOK: vault.elapsed not set${COLOR_reset}"
  exit 1
fi

# Verify elapsed is a positive integer
if echo "${elapsed}" | grep -qE '^[0-9]+$' && [ "${elapsed}" -gt 0 ]; then
  echo -e "${COLOR_green}OK: elapsed time is ${elapsed}ms (positive)${COLOR_reset}"
else
  echo -e "${COLOR_red}NOK: elapsed '${elapsed}' is not a positive integer${COLOR_reset}"
  exit 1
fi

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff -- client-provision.json
