#!/bin/bash

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

# $1: method; $2: url; $3: expected response body
# Variables (reset applied at the end):
#   EXPECTED_STATUS_CODE: expected http status code
#   EXPECTED_BODY: expected reponse body
#   CURL_OPTS: additional curl options
#   ASSERT_IGNORED_FIELDS: ignored nodes at body validation
test_query() {

  local info=$1
  local method=$2
  local url=$3

  local rc=0

  echo
  echo
  echo "=========================================================================="
  echo -e "${info}"
  echo "=========================================================================="
  local cmd="curl -i -X${method} --http2-prior-knowledge ${CURL_OPTS} ${url}"
  echo "Command to be executed:"
  echo "   ${cmd}"
  echo
  [ -n "${EXPECTED_STATUS_CODE}" ] && echo "Expected HTTP status code: ${EXPECTED_STATUS_CODE}"
  if [ -n "${EXPECTED_BODY}" ]
  then
    echo "Expected response body:"
    echo ${EXPECTED_BODY} | jq '.'
  fi
  echo "--------------------------------------------------------------------------"
  echo -n "Press ENTER to execute ... "
  read -r dummy
  echo

  eval ${cmd} > ${TMPDIR}/cmd.out 2>/dev/null
  cat ${TMPDIR}/cmd.out
  echo
  echo "--------------------------------------------------------------------------"

  local receivedStatusCode=$(grep "^HTTP/2" ${TMPDIR}/cmd.out | awk '{ print $2 }')
  local receivedBody=$(grep ^{ ${TMPDIR}/cmd.out)

  if [ -n "${EXPECTED_STATUS_CODE}" ]
  then
    [ "${EXPECTED_STATUS_CODE}" != "${receivedStatusCode}" ] && rc=1 && echo "Expected status code differs from received: ${receivedStatusCode}"
  fi

  local jq_expr="."
  if [ -n "${EXPECTED_BODY}" ]
  then
    # Normalize with jq and ignore 'time' node, and optionally, 'phone' (it is random for unassigned):
    [ -n "${ASSERT_IGNORED_FIELDS}" ] && jq_expr="del(${ASSERT_IGNORED_FIELDS})"
    [ "$(echo ${EXPECTED_BODY} | jq '"${jq_expr}"')" != "$(echo ${receivedBody} | jq '"${jq_expr}"')" ] && rc=1 && echo "Expected body differs from received: ${receivedBody}"
  fi

  if [ ${rc} -eq 0 ]
  then
    echo -n "Response is valid ! "
    [ -n "${ASSERT_IGNORED_FIELDS}" ] && echo -n "(ignored fields: ${jq_expr})"
    echo
  else
    echo "Response is invalid !! Exiting ..."
    echo "Check if the program is started !!"
    exit 1
  fi
  echo "Press ENTER to continue with the next step ... "
  read -r dummy

  # Cleanup
  EXPECTED_STATUS_CODE=
  EXPECTED_BODY=
  CURL_OPTS=
  ASSERT_IGNORED_FIELDS=
}

#############
# EXECUTION #
#############
cd $(dirname $0)

# Requirements:
if ! type curl &>/dev/null; then echo "ERROR: tool 'curl' (with http2 support) must be installed (sudo apt install curl) !" ; return 1 ; fi
if ! type jq &>/dev/null; then echo "ERROR: tool 'jq' must be installed (sudo apt install jq) !" ; return 1 ; fi

# Temporary directory:
TMPDIR=$(mktemp -d)
trap "rm -rf ${TMPDIR}" EXIT

EXPECTED_BODY="{ \"result\":\"true\", \"response\": \"server-matching operation; valid schema and matching data received\" }"
EXPECTED_STATUS_CODE=201
CURL_OPTS="-d'{ \"algorithm\":\"PriorityMatchingRegex\" }' -H \"Content-Type: application/json\""
test_query "Step 1. Server matching configuration" POST http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching

EXPECTED_BODY="{\"algorithm\":\"PriorityMatchingRegex\"}"
EXPECTED_STATUS_CODE=200
test_query "Step 2. Check server matching configuration" GET http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching

test_query "Step 3. Clear possible previous provisions" DELETE http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision

EXPECTED_BODY="{ \"result\":\"true\", \"response\": \"server-provision operation; valid schemas and provisions data received\" }"
EXPECTED_STATUS_CODE=201
CURL_OPTS="-d@./provisions.json -H \"Content-Type: application/json\""
test_query "Step 4. Configure demo provisions" POST http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision

EXPECTED_STATUS_CODE=200
test_query "Step 5. Check server provisions configuration" GET http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision

KNOWN_BODY_REGISTERS=()
KNOWN_BODY_REGISTERS+=( "{\"id\":\"id-1\",\"name\":\"Jess Glynne\",\"phone\":66453}" )
KNOWN_BODY_REGISTERS+=( "{\"developer\":true, \"id\":\"id-2\",\"name\":\"Bryan Adams\",\"phone\":55643}" )
KNOWN_BODY_REGISTERS+=( "{\"id\":\"id-3\",\"name\":\"Phil Collins\",\"phone\":32459}" )
for n in 1 2 3
do
  EXPECTED_BODY=${KNOWN_BODY_REGISTERS[$((n-1))]}
  EXPECTED_STATUS_CODE=200
  ASSERT_IGNORED_FIELDS=".time"
  test_query "Step 6.${n}. Request the identifier 'id-${n}'" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-${n}")
done

EXPECTED_BODY="{\"developer\":true, \"id\":\"id-74\",\"name\":\"unassigned\"}"
EXPECTED_STATUS_CODE=200
ASSERT_IGNORED_FIELDS=".time,.phone"
test_query "Step 7. Request unassigned id-74 (even identifier must carry 'developer': true)" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-74")

EXPECTED_BODY="{\"id\":\"id-75\",\"name\":\"unassigned\"}"
EXPECTED_STATUS_CODE=200
ASSERT_IGNORED_FIELDS=".time,.phone"
test_query "Step 8. Request unassigned id-75 (odd identifier must omit 'developer' field in response)" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-75")

EXPECTED_BODY="{\"cause\":\"invalid workplace id provided, must be in format id-<2-digit number>\"}"
EXPECTED_STATUS_CODE=400
test_query "Step 9. Request invalid id-112" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-112")

EXPECTED_BODY="{\"cause\":\"invalid workplace id provided, must be in format id-<2-digit number>\"}"
EXPECTED_STATUS_CODE=400
test_query "Step 10. Request invalid id-xyz" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-xyz")

for n in 1 2 3
do
  EXPECTED_STATUS_CODE=204
  test_query "Step 11. Delete id-${n}" DELETE http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-${n}")
done

for n in 1 2 3
do
  EXPECTED_STATUS_CODE=404
  test_query "Step 12. Confirm that id-${n} has been deleted" GET http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-${n}")
done

echo
echo "================"
echo "Demo finished OK"
echo "================"
echo

