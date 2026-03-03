#!/bin/bash
# request.json generator
SCR_DIR=$(dirname $0)

random0_512=$(shuf -i 0-512 -n 1)
random0_2=$(shuf -i 0-2 -n 1)
randomString=true
[ "$random0_2" -eq 1 ] && randomString="\"abcde\""
[ "$random0_2" -eq 2 ] && randomString="\"ABCDE\""

cat << EOF > ${SCR_DIR}/request.json
{
  "method": "POST",
  "uri": "/validate",
  "body": {
    "num": ${random0_512},
    "name": ${randomString}
  },
  "headers": [
    "content-type:application/json"
  ]
}
EOF

