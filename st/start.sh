#!/bin/bash
cd $(dirname $0)

ITERATIONS=100000
CONCURRENT_STREAMS=$(nproc --all)
CLIENTS=1

which h2load &>/dev/null || { echo "Required 'h2load' tool (https://nghttp2.org/documentation/h2load-howto.html)" ; exit 1 ; }

curl -i -XPOST --http2-prior-knowledge -d@provision.json -H 'content-type: application/json' http://localhost:8074/admin/v1/server-provision
echo
curl -i -XPUT --http2-prior-knowledge "http://localhost:8074/admin/v1/server-data/configuration?discard=true&discardRequestsHistory=true"
echo
curl -i -XGET --http2-prior-knowledge "http://localhost:8074/admin/v1/server-data/configuration"
echo
echo
echo "Press ENTER to start, CTRL-C to abort ..."
read -r dummy
time h2load -n${ITERATIONS} -c${CLIENTS} -m${CONCURRENT_STREAMS} http://localhost:8000/load-test/v1/id-21 -d ./request.json | tee -a ./report_$(date +'%Y.%m.%d_%H.%M.%S').txt

