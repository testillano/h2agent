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
usage() {
  cat << EOF
Usage: $0 [directory to check, all by default]

       Prepend variables:

       INTERACT: non-empty value exposes interactive inputs. Disabled by default.
EOF
}

# Performs cleanup, and matching/provision configuration
# $1: 's' to specific plural for multiple provisions
cleanup_matching_provision() {
  local s=$1

  EXPECTED_STATUS_CODES="200 204"
  test_query "Server data cleanup" DELETE http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-data || exit 1
  test_query "Provision cleanup" DELETE http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision || exit 1

  if [ -f "matching.json" ]
  then
    EXPECTED_RESPONSE="{ \"result\":\"true\", \"response\": \"server-matching operation; valid schema and matching data received\" }"
    EXPECTED_STATUS_CODES=201
    CURL_OPTS="-d@matching.json -H \"Content-Type: application/json\""
    test_query "Matching configuration" POST http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching || exit 1
  fi

  if [ -f "provision.json" ]
  then
    EXPECTED_RESPONSE="{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema${s} and provision${s} data received\" }"
    EXPECTED_STATUS_CODES=201
    CURL_OPTS="-d@provision.json -H \"Content-Type: application/json\""
    test_query "Provision configuration" POST http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision || exit 1
  fi
}
export -f cleanup_matching_provision

#############
# EXECUTION #
#############
cd $(dirname $0)
echo

[ "$1" = "-h" -o "$1" = "--help" ] && usage && exit 0
DIRECTORIES=$(ls */test.sh | xargs -L1 dirname)
if [ -n "$1" ]
then
  [ ! -d "$1" ] && echo "Missing directory '$1' !" && exit 1
  [ ! -x "$1/test.sh" ] && echo "Invalid test directory '$1' (missing 'test.sh' file) !" && exit 1
  DIRECTORIES=$1
fi

# Temporary directory:
TMPDIR=$(mktemp -d)
trap "rm -rf ${TMPDIR}" EXIT
#trap "echo TMPDIR: ${TMPDIR}" EXIT

# Load test functions:
source ../tools/common.src

# Disable interactiveness and verbose output:
INTERACT=${INTERACT}

# Export variables used in test.src:
export TMPDIR INTERACT
export H2AGENT_ADMIN_ENDPOINT H2AGENT_TRAFFIC_ENDPOINT
export COLOR_reset COLOR_red COLOR_magenta
export -f title test_query

# Iterate tests:
RC=0
PASSED_COUNT=0
TOTAL_COUNT=0
for dir in ${DIRECTORIES}
do
  ${dir}/test.sh
  if [ $? -eq 0 ]
  then
    verdict=passed
    PASSED_COUNT=$((PASSED_COUNT+1))
  else
    verdict=FAILED
    RC=1
  fi
  TOTAL_COUNT=$((TOTAL_COUNT+1))
  echo "[${verdict}] ${dir}" >> ${TMPDIR}/summary.txt
done

echo
echo "======================="
echo "KATA EVALUATION SUMMARY"
echo "======================="
echo
cat ${TMPDIR}/summary.txt
echo
if [ ${RC} -eq 0 ]
then
  echo "=> All OK !"
else
  echo "=> KATA not passed !"
fi
echo
echo "[Score: ${PASSED_COUNT}/${TOTAL_COUNT}]"
echo

exit ${RC}

