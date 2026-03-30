#!/bin/bash
# Test cps pause/resume behavior
# Usage: ./run.sh
#
# Prerequisites: h2agent running on port 8074 (admin)

cd "$(dirname "$0")"
ADMIN="http://localhost:8074/admin/v1"
CURL="curl -s --http2-prior-knowledge"

echo "=== Loading client endpoint ==="
${CURL} -XPOST -H 'content-type:application/json' -d @client-endpoint.json ${ADMIN}/client-endpoint
echo

echo "=== Loading client provision ==="
${CURL} -XPOST -H 'content-type:application/json' -d @client-provision.json ${ADMIN}/client-provision
echo

echo
echo "=== Start h2server.py in another terminal: python3 h2server.py ==="
echo
echo "Then try (moderate rates so you can follow the output):"
echo
echo "  # Prepare range 0-999:"
echo "  ${CURL} '${ADMIN}/client-provision/hitFooBar?sequenceBegin=0&sequenceEnd=999'"
echo
echo "  # Start at 2 cps:"
echo "  ${CURL} '${ADMIN}/client-provision/hitFooBar?cps=2'"
echo
echo "  # Pause (keeps position):"
echo "  ${CURL} '${ADMIN}/client-provision/hitFooBar?cps=0'"
echo
echo "  # Resume at 3 cps (no range = resume from where it stopped):"
echo "  ${CURL} '${ADMIN}/client-provision/hitFooBar?cps=3'"
echo
echo "  # Pause again:"
echo "  ${CURL} '${ADMIN}/client-provision/hitFooBar?cps=0'"
echo
echo "  # New range (resets iterator to 100) and sets rate 1:"
echo "  ${CURL} '${ADMIN}/client-provision/hitFooBar?sequenceBegin=100&sequenceEnd=200&cps=1'"
echo
echo "  # Check current state (see dynamics.seq):"
echo "  ${CURL} ${ADMIN}/client-provision | python3 -m json.tool"
echo
