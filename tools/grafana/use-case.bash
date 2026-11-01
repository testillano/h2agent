#!/bin/echo "source me !"
cd $(dirname $(readlink -f ${BASH_SOURCE[0]}))

# h2agent (book store) configuration:
cat << EOF > /tmp/matching.json
{
  "algorithm": "FullMatchingRegexReplace",
  "rgx": "(/)(buy|sold)(/book/)([0-9]+)",
  "fmt": "\$1\$2\$3"
}
EOF

cat << EOF > /tmp/provision.json
[
  {
    "requestMethod": "POST",
    "requestUri": "/sold/book/",
    "responseCode": 201,
    "outState": "purge",
    "transform": [
      {
        "from": {"source": "requestUri"},
        "filter": [{"op": "regexCapture", "rgx": "(/sold/book/)([0-9]+)"}],
        "to": {"target": "var", "name": "uri_parts"}
      },
      {
        "from": {"source": "var", "name": "uri_parts.2"},
        "filter": [{"op": "equalTo", "value": "81"}],
        "to": {"target": "var", "name": "mises"}
      },
      {
        "from": {"source": "var", "name": "uri_parts.2"},
        "filter": [{"op": "equalTo", "value": "99"}],
        "to": {"target": "var", "name": "hayek"}
      },
      {
        "from": {
          "source": "literal",
          "value": "Book: Human Action | Author: Von Mises, Ludwig | Cost: 51 euro\n"
        },
        "filter": [{"op": "conditionVar", "name": "mises"}],
        "to": {"target": "txtFile", "file": "/tmp/sales.log"}
      },
      {
        "from": {
          "source": "literal",
          "value": "Book: The Road To Serfdom | Author: Hayek, F.A. | Cost: 14 euro\n"
        },
        "filter": [{"op": "conditionVar", "name": "hayek"}],
        "to": {"target": "txtFile", "file": "/tmp/sales.log"}
      }
    ]
  },
  {
    "requestMethod": "GET",
    "requestUri": "/buy/book/",
    "responseCode": 202,
    "outState": "purge",
    "responseBody": {"message": "Sorry we don't have this book, but we will try to bring it as soon as possible !"},
    "responseHeaders": {"content-type": "application/json"},
    "transform": [
      {
        "from": {"source": "requestUri"},
        "filter": [{"op": "regexCapture", "rgx": "(/buy/book/)([0-9]+)"}],
        "to": {"target": "var", "name": "uri_parts"}
      },
      {"from": {"source": "literal", "value": ""}, "to": {"target": "var", "name": "mises"}},
      {
        "from": {"source": "math", "expression": "100/5"},
        "to": {"target": "var", "name": "debeser20"}
      },
      {
        "from": {"source": "math", "expression": "1/5"},
        "to": {"target": "var", "name": "debeser0,2"}
      },
      {
        "from": {"source": "math", "expression": "@{uri_parts.2} % 100"},
        "filter": [{"op": "equalTo", "value": "81"}],
        "to": {"target": "var", "name": "mises"}
      },
      {"from": {"source": "literal", "value": ""}, "to": {"target": "var", "name": "hayek"}},
      {
        "from": {"source": "math", "expression": "@{uri_parts.2} % 100"},
        "filter": [{"op": "equalTo", "value": "99"}],
        "to": {"target": "var", "name": "hayek"}
      },
      {
        "from": {
          "source": "literal",
          "value": "{\"isbn\":\"978-1614273547\",\"book\":\"Human Action\",\"author\":\"Von Mises, Ludwig\",\"message\":\"Accounts debt is: 51 euros\"}"
        },
        "filter": [{"op": "conditionVar", "name": "mises"}, {"op": "cast", "type": "json"}],
        "to": {"target": "responseBody"}
      },
      {
        "from": {
          "source": "literal",
          "value": "{\"isbn\":\"978-0226320595\",\"book\":\"The Road To Serfdom\",\"author\":\"Hayek, F.A.\",\"message\":\"Accounts debt is: 14 euros\"}"
        },
        "filter": [{"op": "conditionVar", "name": "hayek"}, {"op": "cast", "type": "json"}],
        "to": {"target": "responseBody"}
      },
      {
        "from": {"source": "literal", "value": "POST|sold/book/81"},
        "filter": [{"op": "conditionVar", "name": "mises"}],
        "to": {"target": "udpSocket", "socket": "/tmp/udp.sock"}
      },
      {
        "from": {"source": "literal", "value": "POST|sold/book/99"},
        "filter": [{"op": "conditionVar", "name": "hayek"}],
        "to": {"target": "udpSocket", "socket": "/tmp/udp.sock"}
      },
      {
        "from": {"source": "literal", "value": "@{mises}@{hayek}"},
        "to": {"target": "var", "name": "found"}
      },
      {
        "from": {"source": "literal", "value": "Request order: @{uri_parts.2}\n"},
        "filter": [{"op": "conditionVar", "name": "found", "not": true}],
        "to": {"target": "txtFile", "file": "/tmp/lost_sales.log"}
      },
      {
        "from": {"source": "literal", "value": "200"},
        "filter": [{"op": "conditionVar", "name": "found"}],
        "to": {"target": "responseStatusCode"}
      }
    ]
  }
]
EOF

# Cleanup:
echo
echo "Cleaning up ..."
echo
docker kill book-store-container customer-h2-container customer-udp-container 2>/dev/null
docker rm book-store-container customer-h2-container customer-udp-container 2>/dev/null
echo
echo "Press ENTER to start traffic load, CTRL-C to abort ..."
read -r dummy

# h2agent (book store) start:
histogram_latency_opts=(--prometheus-response-delay-seconds-histogram-boundaries "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3")
histogram_size_opts=(--prometheus-message-size-bytes-histogram-boundaries "10,20,100")
opts=(--name book-store --verbose)
docker volume rm BookStoreVol || { echo "Stop containers using the volume !" ; return 1 ; }
docker volume create BookStoreVol
docker run --name book-store-container -t -d --network=host -v BookStoreVol:/tmp ghcr.io/testillano/h2agent:latest "${opts[@]}" "${histogram_latency_opts[@]}" "${histogram_size_opts[@]}"
sleep 1
curl -i --http2-prior-knowledge -XPOST -d@/tmp/matching.json -H content-type:application/json http://localhost:8074/admin/v2/server-matching
curl -i --http2-prior-knowledge -XPOST -d@/tmp/provision.json -H content-type:application/json http://localhost:8074/admin/v2/server-provision

# Customer:
# 1. udp-server-h2client start:
opts=(--udp-socket-path /tmp/udp.sock --name customer --print-each 1 --method "@{udp.1}" --uri "http://0.0.0.0:8000/@{udp.2}" --body '{"foo":"bar"}')
docker run --name customer-h2-container -t -d --network=host -v BookStoreVol:/tmp --entrypoint "/opt/udp-server-h2client" ghcr.io/testillano/h2agent:latest "${opts[@]}" "${histogram_latency_opts[@]}" "${histogram_size_opts[@]}"

# 2. udp-client start:
opts=(--udp-socket-path /tmp/udp.sock --print-each 1 --eps 10 --template "GET|buy/book/@{seq}")
docker run --name customer-udp-container -t -d --network=host -v BookStoreVol:/tmp --entrypoint "/opt/udp-client" ghcr.io/testillano/h2agent:latest "${opts[@]}"

# Blocking log:
docker logs -f customer-h2-container

