#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)

title "$(dirname $0)" "${COLOR_magenta}"
cleanup_server_matching_server_provision

EXPECTED_STATUS_CODES=200
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/ctrl/v2/id-555112233/ts-5555555555" || exit 1

