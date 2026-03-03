#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.bash

h2agent_server_configuration

number1=${RANDOM}
number2=${RANDOM}
number3=${RANDOM}
appVersion=1.0.1

EXPECTED_STATUS_CODES=200
CURL_OPTS="-d'{ \"number1\":\"${number1}\", \"number2\":\"${number2}\", \"number3\":\"${number3}\" }' -H \"content-type: application/json\" -H \"app-version: ${appVersion}\""
EXPECTED_RESPONSE="{ \"number1\":\"${number1}\", \"number2\":\"${number2}\", \"number3\":\"${number3}\", \"processed\": { \"appVersion\": \"${appVersion}\", \"numbers\": [ ${number1}, ${number2}, ${number3} ] } }"
test_query "Send POST request" POST "http://${H2AGENT_TRAFFIC_ENDPOINT}/process/numbers" || exit 1

