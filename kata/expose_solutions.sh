#!/bin/bash
# Password in private repository testillano/h2agent-passwords
PASSWORD="${1:?Usage: $0 <password>}"

# Activate protection hook:
git config core.hooksPath kata/.githooks 2>/dev/null

SCR_DIR=$(readlink -f "$(dirname "$0")")
cd ${SCR_DIR}

find . -name '*.enc' | while read -r enc; do
  target="${enc%.enc}"
  openssl enc -aes-256-cbc -d -pbkdf2 -in "$enc" -out "$target" -pass pass:"$PASSWORD" && echo "Exposed: $target"
done
