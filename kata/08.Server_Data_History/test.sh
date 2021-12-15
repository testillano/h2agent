#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)

title "$(dirname $0)" "${COLOR_magenta}"
cleanup_matching_provision "s"

number=${RANDOM}

EXPECTED_STATUS_CODES=200
CURL_OPTS="-d'{ \"foo\":\"bar\", \"lorem\":\"ipsum\" }' -H \"Content-Type: application/json\""
test_query "Send POST update request" POST "http://${H2AGENT_TRAFFIC_ENDPOINT}/ctrl/v2/items/update/id-${number}" || exit 1

EXPECTED_STATUS_CODES=200
EXPECTED_RESPONSE="{ \"foo\":\"bar\", \"lorem\":\"ipsum\" }"
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/ctrl/v2/items/id-${number}" || exit 1

