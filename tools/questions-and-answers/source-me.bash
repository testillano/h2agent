#!/bin/echo "source me !"

export QDRANT_URL="http://192.168.100.205:6333"

cat << EOF

  * To work on local Qdrant Database, for example:
    $ export QDRANT_URL="http://192.168.100.205:6333"
    $ curl -X GET ${QDRANT_URL}/collections -H "api-key: \$QDRANT_API_KEY" # check collections

  * To work on local Chroma Database:
    $ unset QDRANT_URL

  * Execute 'run.py' help for more detail:
    $ python3 run.py --help

EOF
