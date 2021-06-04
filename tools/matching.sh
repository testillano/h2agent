#!/bin/bash
cd $(dirname $0)

echo
echo "Available matching files:"
echo
ls -1 ../ct/src/resources/server-matching*.json
ls -1 server-matching*.json
echo
echo "Copy/paste the one desired:"
read file
echo
cat $file 2>/dev/null
[ $? -ne 0 ] && echo "Unexpected error !" && exit 1
echo
echo "Press ENTER to confirm, CTRL-C to abort ..."
echo "(remember to execute the h2agent (i.e.: ../build/Release/bin/h2agent -l Debug --verbose))"
read -r dummy

set -x
curl -v -XPOST --http2-prior-knowledge -d @${file} -H "Content-Type: application/json" http://localhost:8074/provision/v1/server-matching

