#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)

title "$(dirname $0)" "${COLOR_magenta}"
cleanup_matching_provision

EXPECTED_STATUS_CODES=200
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/one/uri/path?name=Hayek&city=Friburgo" || exit 1

