#!/bin/bash

if [ -z "$1" ]
then
  echo "Usage:   $0 <microseconds> [request on-the-fly: defaults to 10] [max streams: defaults to 'concurrent requests'] [timeout: none by default (0)]"
  echo "Example: $0 1000000 1000 1000 3"
  exit 1
fi

microseconds=$1
requests=${2:-10}
streams=${3:-${requests}}
timeout=${4:-0}

source helpers.bash &>/dev/null

json=$(mktemp)
cat << EOF > ${json}
{
  "requestMethod": "GET",
  "requestUri": "/foo/bar",
  "responseCode": 200,
  "responseDelayMs": 20,
  "responseHeaders": {
    "content-type": "text/html"
  },
  "responseBody": "done!",
  "transform": [
    {
      "source": "recvseq",
      "target": "var.recvseq"
    },
    {
      "source": "value.${microseconds}",
      "target": "globalVar.ResponseDelayTimerUS.ReceptionId.@{recvseq}"
    }
  ]
}
EOF

server_provision --clean &>/dev/null
server_provision ${json} &>/dev/null
rm ${json}

s_timeout=
[ "${timeout}" != "0" ] && s_timeout="-N${timeout}s"

cat << EOF

Execute this:

   $ h2load -n${requests} -m${streams} ${s_timeout} http://localhost:8000/foo/bar

   In other terminal, you could see ${requests} global variables which indicate that
   ${requests} timers are repeated each ${microseconds} microseconds:

   $ source helpers.bash
   $ global_variable

   Wait for timeout, or force response removing blockers:

   $ global_variable --clean


EOF

