#!/bin/bash
# Interactive request generator for headers validation example
SCR_DIR=$(dirname $0)

echo
echo "Enter header values ('null' to omit):"
read -rp "  x-api-key [my-secret-key]: " api_key
read -rp "  x-client-id [service-a]: " client_id
echo

[ -z "$api_key" ] && api_key="my-secret-key"
[ -z "$client_id" ] && client_id="service-a"

headers=""
h=()
[ "$api_key" != "null" ] && h+=("\"x-api-key:${api_key}\"")
[ "$client_id" != "null" ] && h+=("\"x-client-id:${client_id}\"")

if [ ${#h[@]} -gt 0 ]; then
  headers=$(IFS=,; echo "${h[*]}")
  headers="\"headers\": [${headers}],"
fi

cat << EOF > ${SCR_DIR}/request.json
{
  "method": "GET",
  "uri": "/api/v1/resource",
  ${headers}
  "body": null
}
EOF
