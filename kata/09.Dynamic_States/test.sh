#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)

title "$(dirname $0)" "${COLOR_magenta}"
cleanup_matching_provision "s"

direction=up
for i in 1 2 3 4 5 4 3 2 1
do
  [ $i -eq 5 ] && direction=down

  EXPECTED_STATUS_CODES=200
  CURL_OPTS="-d'{ \"direction\": \"${direction}\" }' -H \"Content-Type: application/json\""
  test_query "Send POST request" POST "http://${H2AGENT_TRAFFIC_ENDPOINT}/evolve" || exit 1
  ITEM_NUMBER=$(grep ^item-number ${TMPDIR}/cmd.out | cut -d: -f2 | xargs)
  cat ${TMPDIR}/cmd.out
  if [ "${ITEM_NUMBER}" != "$i" ]
  then
    title "item number ${ITEM_NUMBER} != $i" "" "x"
    exit 1
  else
    title "item number ${ITEM_NUMBER} is correct" "" "+"
  fi
done


