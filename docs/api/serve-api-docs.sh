#!/bin/bash
# Serves the OpenAPI spec locally using Redoc
# Usage: ./serve-api-docs.sh [port]

PORT=${1:-8001}
DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Serving API docs at http://localhost:${PORT}"
echo "Press Ctrl+C to stop"

docker run --rm -p ${PORT}:80 \
  -v "${DIR}/openapi.yaml:/usr/share/nginx/html/openapi.yaml:ro" \
  -v "${DIR}/index.html:/usr/share/nginx/html/index.html:ro" \
  nginx:alpine
