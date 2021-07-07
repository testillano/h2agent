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
list_matching() {
  echo
  echo "====================================="
  echo "Current server matching configuration"
  echo "====================================="
  curl -s -XGET --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching | jq '.'
  echo
}

list_provisions() {
  echo
  echo "======================================="
  echo "Current server provisions configuration"
  echo "======================================="
  curl -s -XGET --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision | jq '.'
  echo
}

list_data() {
  echo
  echo "==================="
  echo "Current server data"
  echo "==================="
  curl -s -XGET --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-data | jq '.'
  echo
}

list_all() {
  list_matching
  list_provisions
  list_data
}

nextState() {
  case ${MENU_STATE} in
    ConfigureMatching) MENU_STATE=ConfigureProvision ;;
    ConfigureProvision) MENU_STATE=TestTraffic ;;
    TestTraffic) MENU_STATE=ConfigureMatching ;;
  esac
}

menu() {

  local pdir=
  local operation=

  # Initial state
  case ${MENU_STATE} in
    ConfigureMatching) pdir=server-matching ; operation=server-matching ;;
    ConfigureProvision) pdir=tests ; operation=server-provision ;;
    TestTraffic) pdir=tests ;;
  esac

  local files=( $(ls -1 ${pdir}/* 2>/dev/null) )
  local action=$(echo ${MENU_STATE} | sed 's/[A-Z]/ \L&/g')

  echo
  echo "List of files:"
  echo
  count=1
  for file in ${files[@]}
  do
    echo " ${count}. $file"
    count=$((count+1))
  done
  echo
  [ "${MENU_STATE}" = "ConfigureProvision" ] && echo " r. Reset"
  echo " s. Skip"
  echo " 0. Exit"
  echo
  [ "${MENU_STATE}" = "ConfigureMatching" ] && OPT__DFLT=2 # all the tests here are designed for this option (at the moment)
                                                           # (FullMatching_passby_qparams.json is the second alphabetically)
  [ "${MENU_STATE}" = "ConfigureProvision" ] && OPT__DFLT=s
  echo "Input option to${action} [${OPT__DFLT}]:"
  read opt
  [ -z "${opt}" ] && opt=${OPT__DFLT}
  [ "${opt}" = "0" ] && exit 0
  [ "${opt}" = "s" ] && nextState && return 0

  if [ "${MENU_STATE}" = "ConfigureProvision" -a "${opt}" = "r" ]
  then
    echo "Press ENTER to confirm deletion for provisions and internal data ..."
    read -r dummy
    curl -XDELETE --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/${operation}s
    echo "Done !"
    return 0
  fi

  local file
  re='^[0-9]+$'
  if [[ ${opt} =~ $re ]]
  then
    indx=$((opt-1))
    file=${files[${indx}]}
  fi

  [ -z "${file}" ] && echo "Invalid option" && return 0
  local dataFile=$(dirname ${file})/.$(basename ${file})
  cp ${file} ${dataFile} # default for server-matching
  [ "${MENU_STATE}" = "ConfigureProvision" ] && { cat ${file}; OPT__DFLT=${opt}; }

  if [ "${MENU_STATE}" != "ConfigureMatching" ]
  then
    dataFile+=".json"
    sed -n '/^PROVISION/,/^REQUEST_BODY/p' ${file} | grep -vE '^PROVISION|^REQUEST_BODY' | sed '/^$/d' > ${dataFile} # provision
    local method=$(jq -r '.requestMethod' ${dataFile})
    local uri=$(jq -r '.requestUri' ${dataFile})
  fi

  if [ "${MENU_STATE}" = "TestTraffic" ]
  then
    # Traffic body:
    sed -n '/^REQUEST_BODY/,/^REQUEST_HDRS/p' ${file} | grep -vE '^REQUEST' | sed '/^$/d' > ${dataFile} # traffic
    local curlDataOpt=
    [ -s "${dataFile}" ] && curlDataOpt="-d@${dataFile}"

    # headers
    local headers='-H "Content-Type: application/json" '
    [ "${method}" = "GET" ] && headers=
    for hdr in $(sed -n '/^REQUEST_HDRS/,//p' ${file} | grep -vE '^REQUEST')
    do
      headers+=" -H ${hdr}"
    done

    echo
    echo "Method:         ${method}"
    echo "Uri:            ${uri}"
    echo "Headers:        ${headers}"
    echo "Curl data Opts: ${curlDataOpt}"
    echo
    echo "Press ENTER to test traffic, CTRL-C to abort ..."
    read -r dummy
    set -x
    curl -i -X${method} --http2-prior-knowledge ${curlDataOpt} ${headers} http://${H2AGENT_TRAFFIC_ENDPOINT}${uri}
    set +x
    echo "Press ENTER to continue ..."
    read -r dummy
  else
    echo
    echo "Press ENTER to confirm, CTRL-C to abort ..."
    read -r dummy
    # -XPOST not necessary (already inferred)
    set -x
    curl -i --http2-prior-knowledge -d @${dataFile} -H "Content-Type: application/json" http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/${operation}
    set +x
  fi

  nextState
}

#############
# EXECUTION #
#############
cd $(dirname $0)
echo
echo "================================================"
echo "|    NATIVE TEST HELPER UTILITY FOR h2agent    |"
echo "================================================"
echo "Remember to execute the h2agent (i.e.: ../build/Release/bin/h2agent -l Debug --verbose)"
echo
MENU_STATE=ConfigureMatching
while true
do
  list_all
  menu
done

