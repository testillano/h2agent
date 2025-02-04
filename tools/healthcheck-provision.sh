#!/bin/bash
# Useful to quickly provision h2agent with simple successful healthchek request: any GET uri is valid
curl -i --http2-prior-knowledge -XPOST -d'{"requestMethod":"GET","responseCode":200}' -H content-type:application/json http://localhost:8074/admin/v1/server-provision

cat << EOF


Examples:

curl -i --http2-prior-knowledge http://localhost:8000/example/path
h2load -n1000000 -m100 http://localhost:8000/example/path
h2load --h1 -n1000000 -c600 -m1 http://localhost:8001/example/path # if HTTP/1.1 is supported by launched h2agent (https://github.com/nghttp2/nghttp2/issues/333)

EOF

