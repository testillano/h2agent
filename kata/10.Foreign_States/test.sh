#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.src

h2agent_server_configuration

number=${RANDOM}

EXPECTED_STATUS_CODES=200
CURL_OPTS="-d'{ \"foo\":\"bar\", \"lorem\":\"ipsum\" }' -H \"content-type: application/json\""
test_query "Send POST update request" POST "http://${H2AGENT_TRAFFIC_ENDPOINT}/ctrl/v2/items/update/id-${number}" || exit 1

EXPECTED_STATUS_CODES=200
EXPECTED_RESPONSE="{ \"foo\":\"bar\", \"lorem\":\"ipsum\" }"
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/ctrl/v2/items/id-${number}" || exit 1

EXPECTED_STATUS_CODES=200
test_query "Send DELETE request" DELETE "http://${H2AGENT_TRAFFIC_ENDPOINT}/ctrl/v2/items/id-${number}" || exit 1

EXPECTED_STATUS_CODES=404
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/ctrl/v2/items/id-${number}" || exit 1

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff master...kata-solutions -- server-provision.json
