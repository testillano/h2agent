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
# $1: action
press_enter() {
  echo -ne "\n\nPress ENTER ${1}, CTRL-C to abort ..."
  read -r dummy
}

# $1: id
get_uri() {
  echo "/office/v2/workplace?id=$1"
}

#############
# EXECUTION #
#############
cd $(dirname $0)

# Requirements:
if ! type curl &>/dev/null; then echo "ERROR: tool 'curl' (with http2 support) must be installed (sudo apt install curl) !" ; return 1 ; fi
if ! type jq &>/dev/null; then echo "ERROR: tool 'jq' must be installed (sudo apt install jq) !" ; return 1 ; fi

# PriorityMatchingRegex:
press_enter "to configure matching algorithm"
curl --http2-prior-knowledge -d @matching/server-matching.json -H "Content-Type: application/json" http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching
press_enter "to check matching algorithm"
curl --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching

# Provisions:
press_enter "to clear previous provisions" # important for our matching algorithm
curl -XDELETE --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provisions
echo "Done!"

press_enter "to launch demo provisions"
PROVISIONS=( $(ls ./provisions/*) )
echo
for json in ${PROVISIONS[*]}
do
  echo -e "\n\nSending '${json}':"
  cat ${json}
  curl --http2-prior-knowledge -d @${json} -H "Content-Type: application/json" http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provision
  echo -e "\n\nPress ENTER to continue ..."
  read -r dummy
done

press_enter "to check provisions"
curl --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-provisions | jq '.'

for n in 1 2 3
do
  press_enter "to request id-${n}"
  curl -i -XGET --http2-prior-knowledge http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-${n}")
done

press_enter "to request unassigned id-74 (even identifier must carry 'developer': true)"
curl -i -XGET --http2-prior-knowledge http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-74")

press_enter "to request unassigned id-75 (odd identifier must omit 'developer' field in response)"
curl -i -XGET --http2-prior-knowledge http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-75")

press_enter "to request invalid id-112"
curl -i -XGET --http2-prior-knowledge http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-112")

press_enter "to request invalid id-xyz"
curl -i -XGET --http2-prior-knowledge http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-xyz")

press_enter "to delete id-2"
curl -i -XDELETE --http2-prior-knowledge http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-2")

press_enter "to request id-2 and confirm deletion obtaining 404 (Not Found)"
curl -i -XGET --http2-prior-knowledge http://${H2AGENT_TRAFFIC_ENDPOINT}$(get_uri "id-2")

