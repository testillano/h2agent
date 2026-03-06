#!/bin/bash
# Test: reconnect-on-demand when connection is down at send time
#
# Validates that Http2Client::async_send() reconnects and sends the request
# instead of unconditionally marking it as unsent (-1) when the connection
# was initially down.
#
# Prerequisites:
#   - h2agent running on default ports (8000/8074/8080)
#   - docker available to start a target server
#   - Ports 19876/19074 free
#
# Usage: ./test.bash

ADMIN_PORT=${ADMIN_PORT:-8074}
METRICS_PORT=${METRICS_PORT:-8080}
TARGET_TRAFFIC_PORT=19876
TARGET_ADMIN_PORT=19074
TARGET_CONTAINER=h2agent-reconnect-test
H2AGENT_IMAGE=${H2AGENT_IMAGE:-ghcr.io/testillano/h2agent:latest}
HOST=${HOST:-localhost}

CURL="curl -s --http2-prior-knowledge"
PASS=0
FAIL=0

# Prerequisite check
if ! ${CURL} http://${HOST}:${ADMIN_PORT}/admin/v1/configuration &>/dev/null; then
  echo "h2agent not reachable at ${HOST}:${ADMIN_PORT}. Start it first."
  return 1
fi

check() {
  local desc=$1 expected=$2 actual=$3
  if [ "${actual}" = "${expected}" ]; then
    echo "  [PASS] ${desc} (${actual})"
    PASS=$((PASS + 1))
  else
    echo "  [FAIL] ${desc}: expected '${expected}', got '${actual}'"
    FAIL=$((FAIL + 1))
  fi
}

cleanup() {
  echo "Cleaning up ..."
  docker stop ${TARGET_CONTAINER} &>/dev/null || true
  ${CURL} -XDELETE http://${HOST}:${ADMIN_PORT}/admin/v1/client-provision &>/dev/null || true
  ${CURL} -XDELETE http://${HOST}:${ADMIN_PORT}/admin/v1/client-endpoint &>/dev/null || true
}
trap cleanup EXIT

# --- Setup ---
echo "=== Reconnect-on-demand test ==="
cleanup 2>/dev/null

echo
echo "1. Configure client endpoint to port ${TARGET_TRAFFIC_PORT} (nothing listening)"
echo '{"id":"target","host":"'${HOST}'","port":'${TARGET_TRAFFIC_PORT}'}' | \
  ${CURL} -XPOST -d@- -H 'content-type:application/json' http://${HOST}:${ADMIN_PORT}/admin/v1/client-endpoint >/dev/null

echo '{"id":"reconnect-test","endpoint":"target","requestMethod":"GET","requestUri":"/ping","expectedResponseStatusCode":200}' | \
  ${CURL} -XPOST -d@- -H 'content-type:application/json' http://${HOST}:${ADMIN_PORT}/admin/v1/client-provision >/dev/null

sleep 1
status=$(${CURL} http://${HOST}:${ADMIN_PORT}/admin/v1/client-endpoint | python3 -c "import sys,json; print(json.load(sys.stdin)[0].get('status','?'))" 2>/dev/null)
check "Endpoint initially not open" "Closed" "${status}"

# --- Step 1: trigger without server → unsent ---
echo
echo "2. Trigger without server (expect unsent)"
${CURL} "http://${HOST}:${ADMIN_PORT}/admin/v1/client-provision/reconnect-test?sequence=1" >/dev/null
sleep 3

unsent=$(curl -s http://${HOST}:${METRICS_PORT}/metrics | grep 'unsent_counter{source="h2agent_target"' | awk '{print $2}')
check "Unsent counter = 1" "1" "${unsent}"

# --- Step 2: start target server ---
echo
echo "3. Start target server on port ${TARGET_TRAFFIC_PORT}"
docker run -d --rm --name ${TARGET_CONTAINER} --network=host \
  ${H2AGENT_IMAGE} \
  --traffic-server-port ${TARGET_TRAFFIC_PORT} --admin-port ${TARGET_ADMIN_PORT} --prometheus-port 0 --log-level Warning >/dev/null
sleep 3

echo '{"requestMethod":"GET","requestUri":"/ping","responseCode":200,"responseBody":{"status":"ok"}}' | \
  ${CURL} -XPOST -d@- -H 'content-type:application/json' http://${HOST}:${TARGET_ADMIN_PORT}/admin/v1/server-provision >/dev/null

target_ok=$(${CURL} http://${HOST}:${TARGET_TRAFFIC_PORT}/ping 2>/dev/null | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null)
check "Target server responds" "ok" "${target_ok}"

# --- Step 3: trigger again → should reconnect and succeed ---
echo
echo "4. Trigger again (expect reconnect + 200)"
${CURL} "http://${HOST}:${ADMIN_PORT}/admin/v1/client-provision/reconnect-test?sequence=2" >/dev/null
sleep 3

sents=$(curl -s http://${HOST}:${METRICS_PORT}/metrics | grep 'sents_counter{source="h2agent_target"' | awk '{print $2}')
received=$(curl -s http://${HOST}:${METRICS_PORT}/metrics | grep 'received_counter{source="h2agent_target".*status_code="200"' | awk '{print $2}')
status=$(${CURL} http://${HOST}:${ADMIN_PORT}/admin/v1/client-endpoint | python3 -c "import sys,json; print(json.load(sys.stdin)[0].get('status','?'))" 2>/dev/null)

check "Sents counter = 1" "1" "${sents}"
check "Received 200 counter = 1" "1" "${received}"
check "Endpoint now Open" "Open" "${status}"

# --- Summary ---
echo
echo "=== Results: ${PASS} passed, ${FAIL} failed ==="
[ ${FAIL} -eq 0 ] && return 0 || return 1
