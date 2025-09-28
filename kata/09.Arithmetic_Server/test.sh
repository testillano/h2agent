#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.src

h2agent_server_configuration

# random float generator (two decimals):
random_float_between_0_and_10() {
  echo "scale=2; $RANDOM/32768*10" | bc
}

a=$(random_float_between_0_and_10)
b=$(random_float_between_0_and_10)
c=$(random_float_between_0_and_10)
x=$(random_float_between_0_and_10)

EXPECTED_STATUS_CODES=200
EXPECTED_RESPONSE=$(echo "${a}*${x}^2+${b}*${x}+${c}" | bc)
EXPECTED_RESPONSE=$(echo "${EXPECTED_RESPONSE} / 1" | bc) # truncate
title "RANDOM FUNCTION: '${a}*${x}^2 + ${b}*${x} + ${c}' truncated = ${EXPECTED_RESPONSE}" "" "*"
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/calculate/sdpf?a=${a}&b=${b}&c=${c}&x=${x}" || exit 1

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff master...kata-solutions -- server-matching.json server-provision.json
