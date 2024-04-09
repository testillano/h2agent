#!/bin/bash

#############
# VARIABLES #
#############
EXAMPLES=examples
DESCRIPTION=readme.txt
REQUEST=request.json
REQUEST_GENERATOR=request.sh

#############
# FUNCTIONS #
#############
press_enter() {
  echo
  echo "Press ENTER to continue, CTRL+C to abort ..."
  read -r dummy
}

separator() {
  echo "................................................................................"
}

select_option() {
  echo
  echo "Select option to e[x]ecute, (s)napshot, (b)ack to menu, (q)uit [x]:"
  read opt
  [ -z "${opt}" ] && opt=E
  [ "${opt^^}" = "Q" ] && exit 0
  [ "${opt^^}" = "B" ] && return 1
  [ "${opt^^}" = "E" ] && return 0
  if [ "${opt^^}" = "S" ]
  then
     rm -rf ${EXAMPLE}/snapshot
     echo "Collecting information ..."
     snapshot ${EXAMPLE}/snapshot &>/dev/null || exit 1
     echo "Snapshot dump on '${EXAMPLE}/snapshot'"
     return 2
  fi
  select_option
  return $?
}

cleanup() {
  schema --clean
  global_variable --clean
  server_provision --clean
  server_data --clean
  client_endpoint --clean
}

# $1: operation (server-matching, server-provision, etc.); $2: data file path to post
admin_operation() {
  local operation=$1
  local dataFile=$2
  title "${operation}" ${COLOR_green}
  ${CURL} -d @${dataFile} -H "content-type: application/json" ${ADMIN_URL}/${operation} &>/dev/null
  curl -s -XGET --http2-prior-knowledge ${ADMIN_URL}/${operation} | jq '.'
  echo
}

# $1: method; $2: uri; $3: body tmp file; $4: headers
send_request() {
  local method="$1"
  local uri="$2"
  local body="$3"
  local headers="$4"

  local body_opt=
  if [ -n "${body}" ]
  then
    body_opt="-d@${body}"
  fi
  local hdrs_opt=
  for hdr in ${headers}
  do
    hdrs_opt+="-H ${hdr} "
  done

  title "Result" ${COLOR_magenta}
  echo "[${CURL} -X ${method} ${body_opt} ${hdrs_opt} ${TRAFFIC_URL}${uri}]"
  echo
  ${CURL} -X ${method} ${body_opt} ${hdrs_opt} ${TRAFFIC_URL}${uri}
  rm -f ${body}
  echo
}

# $1: example directory
configure_example() {
  echo
  title "$(basename $1)" ${COLOR_green}
  echo
  separator
  cat $1/${DESCRIPTION}
  separator
  echo
  press_enter
  cleanup >/dev/null

  for operation in client-endpoint server-matching server-provision global-variable schema
  do
    local f="$1/${operation}.json"
    [ -f "${f}" ] && admin_operation ${operation} ${f}
  done
}

menu() {
  title "LET's PLAY WITH SOME h2agent EXAMPLES" ${COLOR_blue}
  local files=( $(find ${EXAMPLES} -name ${DESCRIPTION} 2>/dev/null) )
  echo
  echo "Examples available:"
  echo
  count=1
  for file in ${files[@]}
  do
    echo " ${count}. $(basename $(dirname ${file}))"
    count=$((count+1))
  done
  echo
  echo " 0. Exit"
  echo
  echo "Select example [0]:"
  read opt
  [ -z "${opt}" ] && opt=0
  [ "${opt}" = "0" ] && exit 0

  local file
  re='^[0-9]+$'
  if [[ ${opt} =~ $re ]]
  then
    indx=$((opt-1))
    file=${files[${indx}]}
  fi

  [ -z "${file}" ] && echo "Invalid option !" && return 0

  EXAMPLE=$(dirname $file)
  configure_example ${EXAMPLE}
  if [ ! -f "${EXAMPLE}/${REQUEST}" -a ! "${EXAMPLE}/${REQUEST_GENERATOR}" ] # no request means just didactic content
  then
    press_enter # no request means just didactic content
  else
    while true
    do
      title "Send request" ${COLOR_green}
      separator
      [ -x "${EXAMPLE}/${REQUEST_GENERATOR}" ] && ${EXAMPLE}/${REQUEST_GENERATOR}
      local method=$(jq -r '.method' ${EXAMPLE}/${REQUEST})
      local uri=$(jq -r '.uri' ${EXAMPLE}/${REQUEST})
      local body_f=$(mktemp)
      jq -r '.body' ${EXAMPLE}/${REQUEST} > ${body_f}
      local body=$(cat ${body_f})

      # Request summary:
      local headers=$(jq -r '.headers[]' ${EXAMPLE}/${REQUEST} 2>/dev/null)
      echo "Method: ${method}, URI: ${uri}"
      if [ "${body}" != "null" ]
      then
        echo -e "Body:\n${body}"
      else
        body_f=
        rm -f ${body_f}
      fi
      [ -n "${headers}" ] && echo "Headers: $(echo ${headers} | tr '\n' ' ')"

      separator
      select_option
      rc=$?
      [ $rc -eq 0 ] && send_request "${method}" "${uri}" "${body_f}" "${headers}" && press_enter
      [ $rc -eq 1 ] && break
    done
  fi
}

#############
# EXECUTION #
#############
cd $(dirname $0)
echo

# Load common resources:
source ../common.src
source ../helpers.src &>/dev/null

title "H2agent play"

h2agent_check ${TRAFFIC_URL} ${ADMIN_URL} || exit 1

while true
do
  menu
done

