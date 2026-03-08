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

# Step 1: try to enter without knocking (should get 403)
title "Step 1: GET /enter without knocking (expect 403)" "" "*"
EXPECTED_STATUS_CODES=403
test_query "GET /enter (no knock)" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/enter" || exit 1

# Step 2: knock
title "Step 2: POST /knock" "" "*"
EXPECTED_STATUS_CODES=200
test_query "POST /knock" POST "http://${H2AGENT_TRAFFIC_ENDPOINT}/knock" || exit 1

# Step 3: enter (should get 200 now)
title "Step 3: GET /enter after knocking (expect 200)" "" "*"
EXPECTED_STATUS_CODES=200
test_query "GET /enter (after knock)" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/enter" || exit 1

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff -- server-provision.json client-endpoint.json client-provision.json
