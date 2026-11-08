#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.bash

h2agent_server_configuration

# random float generator (two decimals):
random_float_between_0_and_10() {
  awk -v r="${RANDOM}" 'BEGIN { printf "%.2f", int(r * 100 / 32768) / 10 }'
}

a=$(random_float_between_0_and_10)
b=$(random_float_between_0_and_10)
c=$(random_float_between_0_and_10)
x=$(random_float_between_0_and_10)

EXPECTED_STATUS_CODES=200
# truncated; doubles, as the h2agent math source
EXPECTED_RESPONSE=$(awk -v a="${a}" -v b="${b}" -v c="${c}" -v x="${x}" 'BEGIN { printf "%d", int(a * x ^ 2 + b * x + c) }')
title "RANDOM FUNCTION: '${a}*${x}^2 + ${b}*${x} + ${c}' truncated = ${EXPECTED_RESPONSE}" "" "*"
test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/calculate/sdpf?a=${a}&b=${b}&c=${c}&x=${x}" || exit 1

