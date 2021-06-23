#!/bin/bash

#############
# VARIABLES #
#############
# h2agent endpoints
H2AGENT_ADMIN_ENDPOINT=localhost:8074

#############
# FUNCTIONS #
#############
echo
echo "======================"
echo "server-matching schema"
echo "======================"
curl -s -XGET --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v1/server-matching/schema | jq '.'
echo
echo "======================="
echo "server-provision schema"
echo "======================="
curl -s -XGET --http2-prior-knowledge http://localhost:8074/admin/v1/server-provision/schema | jq '.'
echo
echo "=================="
echo "server-data schema"
echo "=================="
curl -s -XGET --http2-prior-knowledge http://localhost:8074/admin/v1/server-data/schema | jq '.'
echo

