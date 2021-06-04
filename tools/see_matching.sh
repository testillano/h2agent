#!/bin/bash
set -x
curl -XGET --http2-prior-knowledge http://localhost:8074/provision/v1/server-matching | jq '.'
