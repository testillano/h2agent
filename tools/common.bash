#!/bin/echo "source me!"
# Source from script

# Requirements:
if ! type nc &>/dev/null; then echo "ERROR: tool 'nc' must be installed (i.e.: sudo apt install netcat) !" ; return 1 ; fi
if ! type curl &>/dev/null; then echo "ERROR: tool 'curl' (with http2 support) must be installed (i.e.: sudo apt install curl) !" ; return 1 ; fi
if ! type jq &>/dev/null; then echo "ERROR: tool 'jq' must be installed (i.e.: sudo apt install jq) !" ; return 1 ; fi
if ! type dos2unix &>/dev/null; then echo "ERROR: tool 'dos2unix' must be installed (i.e.: sudo apt install dos2unix) !" ; return 1 ; fi

# Color variables:
export COLOR_reset='\e[0m'
export COLOR_red='\e[31m'
export COLOR_l_red='\e[91m'
export COLOR_green='\e[32m'
export COLOR_l_green='\e[92m'
export COLOR_yellow='\e[33m'
export COLOR_l_yellow='\e[93m'
export COLOR_blue='\e[34m'
export COLOR_l_blue='\e[94m'
export COLOR_magenta='\e[35m'
export COLOR_l_magenta='\e[95m'
export COLOR_cyan='\e[36m'
export COLOR_l_cyan='\e[96m'

# h2agent endpoints:
export H2AGENT_ADMIN_ENDPOINT=localhost:8074
export H2AGENT_TRAFFIC_ENDPOINT=localhost:8000

# Avoid interactivity with 'git diff' output:
export GIT_PAGER=cat

# $1: title literal; $2: color code (red by default); $3: pattern character (= by default)
title()
{
  local literal=$1
  local color=${2:-${COLOR_red}}
  local pattern=${3:-=}

  local title="${pattern} ${literal} ${pattern}"
  local line=$(eval printf "%0.s${pattern}" {1..${#title}})
  echo -e "${color}${line}\n${title}\n${line}${COLOR_reset}"
}
export -f title

# Temporary for test_query():
if [ -z "${TMPDIR}" ]
then
  export TMPDIR=$(mktemp -d)
  trap "rm -rf ${TMPDIR} ; unset TMPDIR" EXIT
fi

# $1: title; $2: method; $3: url
# Prepend variables
#   (*) EXPECTED_STATUS_CODES: space-separated list of expected http status code(s)
#   (*) EXPECTED_RESPONSE: expected response
#   (*) CURL_OPTS: additional curl options
#   (*) ASSERT_JSON_IGNORED_FIELDS: ignored nodes at json body validation
#       NON_INTERACT: non-empty value disable interactivity
#       TMPDIR: temporary directory (must be defined externally)
#
# (*) reset applied at the end
#
# ${TMPDIR}/cmd.out: stores the stdout for the curl command
# ${TMPDIR}/cmd.err: stores the stderr for the curl command
test_query() {
  local info=$1
  local method=$2
  local url=$3

  [ -z "${TMPDIR}" ] && echo "Temporary directory 'TMPDIR' must be defined !" && return 1

  local rc=0
  echo
  title "${info}" "" "-"
  local cmd="curl -i -X${method} --http2-prior-knowledge ${CURL_OPTS} \"${url}\""
  echo "CURL command: ${cmd}"
  echo
  [ -n "${EXPECTED_STATUS_CODES}" ] && echo "Expected HTTP status code(s): ${EXPECTED_STATUS_CODES}"
  [ -n "${EXPECTED_RESPONSE}" ] && echo "Expected response: ${EXPECTED_RESPONSE}"
  [ -z "${NON_INTERACT}" ] && echo -en "\nPress ENTER to send the request ... " && read -r dummy

  eval ${cmd} > ${TMPDIR}/cmd.out 2>${TMPDIR}/cmd.err
  dos2unix ${TMPDIR}/cmd.out ${TMPDIR}/cmd.out &>/dev/null
  dos2unix ${TMPDIR}/cmd.err ${TMPDIR}/cmd.err &>/dev/null
  echo

  # STATUS CODE
  local receivedStatusCode=$(grep "^HTTP/2" ${TMPDIR}/cmd.out | awk '{ print $2 }')
  echo -n "Received HTTP status code: "
  if [ -n "${receivedStatusCode}" ]
  then
    echo -n "${receivedStatusCode} "
    if [ -n "${EXPECTED_STATUS_CODES}" ]
    then
      echo "${EXPECTED_STATUS_CODES}" | grep -qw "${receivedStatusCode}"
      [ $? -ne 0 ] && rc=1 && echo "=> differs from expected '${EXPECTED_STATUS_CODES}' !"
    fi
    echo
  else
    echo "not received !"
    [ -n "${EXPECTED_STATUS_CODES}" ] && rc=1
  fi

  # RESPONSE
  local receivedResponse=$(tail -1 ${TMPDIR}/cmd.out)
  echo "Received response: ${receivedResponse}"
  local jq_expr="."
  if [ -n "${EXPECTED_RESPONSE}" ]
  then
    # Normalize with jq ignoring optionally fields like 'time' or similar
    [ -n "${ASSERT_JSON_IGNORED_FIELDS}" ] && jq_expr="del(${ASSERT_JSON_IGNORED_FIELDS})"
    expected=$(echo ${EXPECTED_RESPONSE} | jq "${jq_expr}")
    received=$(echo ${receivedResponse} | jq "${jq_expr}")
    [ "${expected}" != "${received}" ] && rc=1 && echo -e "=> differs from expected !\n\nExpected:\n${expected}\nReceived:\n${received}"
  fi

  echo
  if [ ${rc} -eq 0 ]
  then
    echo -n "Response is OK :-)"
    [ -n "${ASSERT_JSON_IGNORED_FIELDS}" ] && echo -n " (ignored fields: ${jq_expr})"
    echo
  else
    echo "Response is NOK :_("
    cat ${TMPDIR}/cmd.err 2>/dev/null
  fi
  if [ -z "${NON_INTERACT}" ]
  then
    echo -en "\nPress ENTER to continue ... "
    read -r dummy
  fi

  # Reset
  EXPECTED_STATUS_CODES=
  EXPECTED_RESPONSE=
  CURL_OPTS=
  ASSERT_JSON_IGNORED_FIELDS=

  # Return rc
  return ${rc}
}
export -f test_query

# Performs cleanup, and server matching/provision configuration
# server-matching.json and server-provision.json must be located at current directory
h2agent_server_configuration() {
  EXPECTED_STATUS_CODES="200 204"
  test_query "Server data cleanup" DELETE http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-data || exit 1
  test_query "Provision cleanup" DELETE http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision || exit 1

  if [ -f "server-matching.json" ]
  then
    EXPECTED_RESPONSE="{ \"result\":\"true\", \"response\": \"server-matching operation; valid schema and matching data received\" }"
    EXPECTED_STATUS_CODES=201
    CURL_OPTS="-d@server-matching.json -H \"content-type: application/json\""
    test_query "Matching configuration" POST http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching || exit 1
  fi

  if [ -f "server-provision.json" ]
  then
    local s=
    [ "$(jq -e 'type == "array"' server-provision.json)" = "true" ] && s=s # plural for arrays

    EXPECTED_RESPONSE="{ \"result\":\"true\", \"response\": \"server-provision operation; valid schema${s} and server provision${s} data received\" }"
    EXPECTED_STATUS_CODES=201
    CURL_OPTS="-d@server-provision.json -H \"content-type: application/json\""
    test_query "Provision configuration" POST http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision || exit 1
  fi
}
export -f h2agent_server_configuration

# $@: URLs to check by mean netcat (user should provide administrative and traffic server addresses)
h2agent_check() {
  for endpoint in $@
  do
    nc -w 1 -z $(echo ${endpoint} | cut -d/ -f3 | sed 's/:/ /') || { echo -e "\nERROR: h2agent must be started ('${endpoint}' not accessible) !" ; return 1 ; }
  done

  return 0
}
export -f h2agent_check
