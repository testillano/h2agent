#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.bash

h2agent_server_configuration

EXPECTED_STATUS_CODES=200
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/ctrl/v2/id-555112233/ts-5555555555" || exit 1

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff master...kata-solutions -- server-provision.json
