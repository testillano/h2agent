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

evaluate() {
  local dir=$1

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
}

menu() {
  echo
  title "Available exercises:"
  for dir in ${DIRECTORIES[@]}
  do
    echo ${dir} | tr '.' ' '
  done
  echo
  echo "Select option (x: exit; a: all) [${LAST_OPTION}]:"
  read opt
  [ "${opt}" = "x" ] && echo "Goodbye !" && exit 0
  [ -z "${opt}" ] && opt=${LAST_OPTION}

  if [ "${opt}" = "a" ]
  then
    for dir in ${DIRECTORIES[@]}; do evaluate ${dir}; done
  else
    local re='^[0-9]+$'
    if ! [[ ${opt} =~ $re ]] ; then
      echo "Invalid option! Press ENTER to continue ..." && read -r dummy && return 1
    fi
    opt=$(echo ${opt} | sed -e 's/^[0]*//') # remove trailing zeroes
    indx=$((opt - 1))
    [ ${indx} -lt 0 -o ${indx} -ge ${#DIRECTORIES[@]} ] && echo "Invalid option! Press ENTER to continue ..." && read -r dummy && return 1
    LAST_OPTION=$((indx + 1))
    evaluate ${DIRECTORIES[${indx}]}
  fi

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
  echo "Press ENTER to continue ..."
  read -r dummy
}

#############
# EXECUTION #
#############
cd $(dirname $0)
echo

[ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: $0 [-i|--interactive]" && exit 0
DIRECTORIES=( $(ls -d */) )
LAST_OPTION=a # execute all

# Temporary directory:
TMPDIR=$(mktemp -d)
trap "rm -rf ${TMPDIR}" EXIT
#trap "echo TMPDIR: ${TMPDIR}" EXIT

# Load test functions:
source ../tools/common.src

# Enable interactiveness if selected:
INTERACT=
[ "$1" = "-i" -o "$1" = "--interactive" ] && INTERACT=true

# Export variables used in test.src:
export TMPDIR INTERACT
export H2AGENT_ADMIN_ENDPOINT H2AGENT_TRAFFIC_ENDPOINT
export COLOR_reset COLOR_red COLOR_magenta
export -f title test_query

while true
do
  0> ${TMPDIR}/summary.txt
  RC=0
  PASSED_COUNT=0
  TOTAL_COUNT=0
  menu
done

