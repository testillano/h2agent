#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.src

h2agent_server_configuration

number=${RANDOM}
next_number=$((number+1))

EXPECTED_STATUS_CODES=$((200 + 200*(number % 2)))
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/number/is/even/${number}" || exit 1

EXPECTED_STATUS_CODES=$((400 - 200*(number % 2)))
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/number/is/even/${next_number}" || exit 1

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff master...kata-solutions -- server-provision.json
