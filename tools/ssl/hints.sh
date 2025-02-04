#!/bin/bash
# Usage: $0 <url> <server key> <server crt> <server password> <client key> <client crt> <client password> <cacert>
# For example: ./hints.sh https://localhost:8074/admin/v1/server-matching
url=$1
skey=$2
scrt=$3
spass=$4
ckey=$5
ccrt=$6
cpass=$7
cacert=$8

cat << EOF


  1. Start the server

  1.1 Natively:
      $ ./h2agent -l Debug --verbose --traffic-server-key ${skey} --traffic-server-crt ${scrt} --secure-admin --traffic-server-key-password ${spass} &

  1.2 Dockerized
      $ XTRA_ARGS="-v \$PWD:/ssl" <project root>/run.sh --traffic-server-key /ssl/${skey} --traffic-server-crt /ssl/${scrt} --secure-admin --traffic-server-key-password ${spass} &

  2. Then test

  2.1 Securely:
      $ curl --http2 --cert ${ccrt} --key ${ckey} --pass ${cpass} --cacert ${cacert} ${url}

  2.2 Insecurely:
      $ curl --insecure --http2-prior-knowledge ${url}
      Using firefox, you may accept the risks:
      $ firefox ${url}

EOF

