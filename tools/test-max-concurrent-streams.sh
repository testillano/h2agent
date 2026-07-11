#!/bin/bash
# Test: verify --traffic-server-max-concurrent-streams works correctly
#
# Strategy: provision a response delay so streams accumulate in-flight.
# With max_concurrent_streams=1 and 1 connection, only 1 stream can be
# in-flight at a time -> throughput is ~1/delay.
# With max_concurrent_streams=100, 100 streams can run in parallel on
# 1 connection -> throughput is ~100/delay.
#
# Requirements: h2load, curl, h2agent running externally (user launches it)

TRAFFIC_PORT="${TRAFFIC_PORT:-8000}"
ADMIN_PORT="${ADMIN_PORT:-8074}"
TRAFFIC="http://localhost:${TRAFFIC_PORT}"
ADMIN="http://localhost:${ADMIN_PORT}"
REQUESTS=50
DELAY_MS=100  # 100ms delay per response

provision() {
    for i in $(seq 1 30); do
        curl -sf --http2-prior-knowledge "${ADMIN}/admin/v1/logging" >/dev/null 2>&1 && break
        sleep 0.2
    done
    curl -sf --http2-prior-knowledge -XPOST "${ADMIN}/admin/v1/server-provision" \
        -H'content-type:application/json' -d'{
        "requestMethod": "GET",
        "requestUri": "/test",
        "responseCode": 200,
        "responseBody": "ok",
        "responseDelayMs": '"${DELAY_MS}"'
    }' >/dev/null
    echo "    Provisioned: GET /test -> 200 with ${DELAY_MS}ms delay"
}

run_load() {
    local max_streams="$1"
    echo ""
    echo "--- h2load: -n${REQUESTS} -c1 -m50 ${TRAFFIC}/test ---"
    RESULT=$(h2load -n${REQUESTS} -c1 -m50 ${TRAFFIC}/test 2>&1)
    echo "${RESULT}" | grep -E "^(finished|requests:|time for request)"
    REQ_S=$(echo "${RESULT}" | grep "^finished" | grep -oP '[\d.]+(?= req/s)')
    eval "REQ_S_${max_streams}=${REQ_S}"
}

echo "============================================================"
echo " Test: --traffic-server-max-concurrent-streams"
echo " Delay per response: ${DELAY_MS}ms | Requests: ${REQUESTS}"
echo " Traffic port: ${TRAFFIC_PORT} | Admin port: ${ADMIN_PORT}"
echo "============================================================"

# --- Test A ---
echo ""
echo "=== Test A: max_concurrent_streams=1 ==="
echo ""
echo "    Please start h2agent with:"
echo ""
echo "      h2agent --traffic-server-max-concurrent-streams 1 --discard-data --disable-metrics"
echo ""
read -rp "    Press ENTER when h2agent is running..."
provision
run_load 1

echo ""
echo "    Please STOP h2agent now."
read -rp "    Press ENTER when stopped..."

# --- Test B ---
echo ""
echo "=== Test B: max_concurrent_streams=100 ==="
echo ""
echo "    Please start h2agent with:"
echo ""
echo "      h2agent --traffic-server-max-concurrent-streams 100 --discard-data --disable-metrics"
echo ""
read -rp "    Press ENTER when h2agent is running..."
provision
run_load 100

echo ""
echo "    Please STOP h2agent now."
read -rp "    Press ENTER when stopped..."

# --- Results ---
echo ""
echo "============================================================"
echo " Results:"
echo "   Test A (max=1):   ${REQ_S_1} req/s"
echo "   Test B (max=100): ${REQ_S_100} req/s"
echo ""

if command -v bc >/dev/null 2>&1 && [ -n "${REQ_S_1}" ] && [ -n "${REQ_S_100}" ]; then
    RATIO=$(echo "scale=1; ${REQ_S_100} / ${REQ_S_1}" | bc 2>/dev/null)
    echo "   Ratio B/A: ${RATIO}x (expected ~50x)"
    if [ "$(echo "${RATIO} > 10" | bc)" -eq 1 ]; then
        echo ""
        echo " PASS: max_concurrent_streams is working correctly."
    else
        echo ""
        echo " FAIL: ratio too low, max_concurrent_streams may not be effective."
        exit 1
    fi
else
    echo "   (Install 'bc' for automatic ratio check)"
fi
echo "============================================================"
