#!/bin/bash
# Trigger and verify
source ../../common.bash
source ../../helpers.bash &>/dev/null

echo "Triggering client provision 'myFlow' ..."
client_provision myFlow
echo
sleep 1
echo "Client events:"
client_data | jq '.'
echo
echo "Server events:"
server_data | jq '.'
echo
echo "Global variables:"
global_variable | jq '.'
