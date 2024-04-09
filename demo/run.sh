#!/bin/bash
# Set INTERACT=false on calling shell to test without prompts

#############
# VARIABLES #
#############
# h2agent endpoints
H2AGENT_ADMIN_ENDPOINT=localhost:8074
H2AGENT_TRAFFIC_ENDPOINT=localhost:8000

#############
# FUNCTIONS #
#############

# $1: id
get_uri() {
  echo "/office/v2/workplace?id=$1"
}

#############
# EXECUTION #
#############
cd $(dirname $0)
echo

# Temporary directory:
TMPDIR=$(mktemp -d)
trap "rm -rf ${TMPDIR}" EXIT

# Load common resources:
source ../tools/common.src

title "H2agent demo"

h2agent_check ${H2AGENT_ADMIN_ENDPOINT} ${H2AGENT_TRAFFIC_ENDPOINT} || exit 1

# Enable interactiveness:
INTERACT=${INTERACT:-true}
[ "${INTERACT}" = "false" ] && INTERACT=

# Initial cleanup
EXPECTED_STATUS_CODES="200 204"
test_query "Initial cleanup" DELETE http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-data || { echo -e "\nCheck that the h2agent application is started" ; exit 1 ; }

EXPECTED_STATUS_CODES="200"
test_query "Enable events" PUT "http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-data/configuration?discard=false&discardKeyHistory=false" || exit 1

EXPECTED_RESPONSE="{ \"result\":\"true\", \"response\": \"server-matching operation; valid schema and matching data received\" }"
EXPECTED_STATUS_CODES=201
CURL_OPTS="-d'{ \"algorithm\":\"RegexMatching\" }' -H \"content-type: application/json\""
test_query "Step 1. Server matching configuration" POST http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching || exit 1

EXPECTED_RESPONSE="{\"algorithm\":\"RegexMatching\"}"
EXPECTED_STATUS_CODES=200
test_query "Step 2. Check server matching configuration" GET http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching || exit 1

test_query "Step 3. Clear possible previous provisions" DELETE http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision || exit 1

EXPECTED_RESPONSE="{ \"result\":\"true\", \"response\": \"server-provision operation; valid schemas and server provisions data received\" }"
EXPECTED_STATUS_CODES=201
CURL_OPTS="-d@./server-provision.json -H \"content-type: application/json\""
test_query "Step 4. Configure demo provisions" POST http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision || exit 1

EXPECTED_STATUS_CODES=200
test_query "Step 5. Check server provisions configuration" GET http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision || exit 1

KNOWN_RESPONSE_REGISTERS=()
KNOWN_RESPONSE_REGISTERS+=( "{\"id\":\"id-1\",\"name\":\"Jess Glynne\",\"phone\":66453}" )
KNOWN_RESPONSE_REGISTERS+=( "{\"developer\":true, \"id\":\"id-2\",\"name\":\"Bryan Adams\",\"phone\":55643}" )
KNOWN_RESPONSE_REGISTERS+=( "{\"id\":\"id-3\",\"name\":\"Phil Collins\",\"phone\":32459}" )
for n in 1 2 3
do
  EXPECTED_RESPONSE=${KNOWN_RESPONSE_REGISTERS[$((n-1))]}
  EXPECTED_STATUS_CODES=200
  ASSERT_JSON_IGNORED_FIELDS=".time"
  test_query "Step 6.${n}. Request the identifier 'id-${n}'" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-${n}") || exit 1
done

EXPECTED_RESPONSE="{\"developer\":true, \"id\":\"id-74\",\"name\":\"unassigned\"}"
EXPECTED_STATUS_CODES=200
ASSERT_JSON_IGNORED_FIELDS=".time,.phone"
test_query "Step 7. Request unassigned id-74 (even identifier must carry 'developer': true)" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-74") || exit 1

EXPECTED_RESPONSE="{\"id\":\"id-75\",\"name\":\"unassigned\"}"
EXPECTED_STATUS_CODES=200
ASSERT_JSON_IGNORED_FIELDS=".time,.phone"
test_query "Step 8. Request unassigned id-75 (odd identifier must omit 'developer' field in response)" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-75") || exit 1

EXPECTED_RESPONSE="{\"cause\":\"invalid workplace id provided, must be in format id-<2-digit number>\"}"
EXPECTED_STATUS_CODES=400
test_query "Step 9. Request invalid id-112" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-112") || exit 1

EXPECTED_RESPONSE="{\"cause\":\"invalid workplace id provided, must be in format id-<2-digit number>\"}"
EXPECTED_STATUS_CODES=400
test_query "Step 10. Request invalid id-xyz" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-xyz") || exit 1

for n in 1 2 3
do
  EXPECTED_STATUS_CODES=204
  test_query "Step 11. Delete id-${n}" DELETE http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-${n}") || exit 1
done

for n in 1 2 3
do
  EXPECTED_STATUS_CODES=404
  test_query "Step 12. Confirm that id-${n} has been deleted" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-${n}") || exit 1
done

echo
title "Demo finished OK"

