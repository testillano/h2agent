#!/bin/bash

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.bash

h2agent_server_configuration

QUOTE=
for part in 1 2 3
do
  EXPECTED_STATUS_CODES=200
  test_query "Send GET request" GET "http://${H2AGENT_TRAFFIC_ENDPOINT}/rothbard/says" || exit 1
  QUOTE+="$(tail -1 ${TMPDIR}/cmd.out | tr -d '"')"
done

echo
title "REASSEMBLED QUOTE: ${QUOTE}" "" "*"
if [ "${QUOTE}" != "The State is, and always has been, the great single enemy of the human race, its liberty, happiness, and progress." ]
then
  title "Murray didn't say that ! :_(" "" "x"
  exit 1
else
  title "Well done !" "" "+"
fi

echo -e "\n\n\n=====================\nTHESE ARE THE CHANGES \n=====================\n"
git diff master...kata-solutions -- server-provision.json
