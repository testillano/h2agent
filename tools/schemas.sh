#!/bin/bash

#############
# VARIABLES #
#############
# h2agent endpoints
H2AGENT_ADMIN_ENDPOINT=localhost:8074

#############
# FUNCTIONS #
#############
for resource in $(curl -s --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v2/api-schema | jq -r '.[]'); do
  echo
  echo "===== ${resource} schema ====="
  curl -s -XGET --http2-prior-knowledge http://${H2AGENT_ADMIN_ENDPOINT}/admin/v2/api-schema/${resource} | jq '.'
done
echo
