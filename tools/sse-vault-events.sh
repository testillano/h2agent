#!/bin/bash
# Example: Subscribe to vault SSE events and write a key to trigger them.
#
# Terminal 1 (listener):
#   ./sse-vault-events.sh listen
#
# Terminal 2 (writer):
#   ./sse-vault-events.sh write
#
# Or run both in one shot (background listener):
#   ./sse-vault-events.sh demo

ADMIN=${H2AGENT_ADMIN_ENDPOINT:-http://localhost:8074}
SSE_URL="${ADMIN}/admin/v1/vault/events"
VAULT_URL="${ADMIN}/admin/v1/vault"

# Check h2agent is reachable
if ! curl --http2-prior-knowledge -sf "${ADMIN}/admin/v1/configuration" -o /dev/null 2>/dev/null; then
  echo "ERROR: h2agent admin endpoint not reachable at ${ADMIN}"
  echo "Start it with: ./run.sh (from project root)"
  exit 1
fi

case "${1:-demo}" in
  listen)
    echo "Listening for vault SSE events on ${SSE_URL}?key=demo_key ..."
    echo "(Press Ctrl+C to stop)"
    echo
    # --http2-prior-knowledge for h2c (no TLS)
    curl --http2-prior-knowledge -N -s "${SSE_URL}?key=demo_key"
    ;;

  write)
    echo "Writing to vault key 'demo_key'..."
    curl --http2-prior-knowledge -s -X POST "${VAULT_URL}" \
      -H "content-type: application/json" \
      -d '{"demo_key": "hello_sse"}'
    echo
    echo "Done. Check listener terminal for the event."
    ;;

  demo)
    echo "=== SSE Vault Events Demo ==="
    echo

    SSE_OUTPUT=$(mktemp)
    trap "rm -f ${SSE_OUTPUT}" EXIT

    echo "Starting SSE listener in background..."
    curl --http2-prior-knowledge -N -s "${SSE_URL}?key=demo_key" > "${SSE_OUTPUT}" &
    LISTENER_PID=$!
    sleep 1

    echo "Writing vault key 'demo_key' = \"event_1\"..."
    curl --http2-prior-knowledge -s -o /dev/null -X POST "${VAULT_URL}" \
      -H "content-type: application/json" \
      -d '{"demo_key": "event_1"}'
    sleep 0.5

    echo "Writing vault key 'demo_key' = \"event_2\"..."
    curl --http2-prior-knowledge -s -o /dev/null -X POST "${VAULT_URL}" \
      -H "content-type: application/json" \
      -d '{"demo_key": "event_2"}'
    sleep 0.5

    kill $LISTENER_PID 2>/dev/null
    wait $LISTENER_PID 2>/dev/null

    echo
    echo "--- SSE events received by listener ---"
    if [ -s "${SSE_OUTPUT}" ]; then
      cat "${SSE_OUTPUT}"
    else
      echo "(none - is h2agent compiled with SSE support?)"
    fi
    ;;

  *)
    echo "Usage: $0 [listen|write|demo]"
    exit 1
    ;;
esac
