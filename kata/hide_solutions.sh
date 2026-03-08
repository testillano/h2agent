#!/bin/bash
# Password in private repository testillano/h2agent-passwords
PASSWORD="${1:?Usage: $0 <password>}"

SCR_DIR=$(readlink -f "$(dirname "$0")")
cd ${SCR_DIR}

# To expose new/modified stuff, just touch <file>.enc and run this script.
# Then run "git add *.enc" and commit changes
find . -name '*.enc' | while read -r enc; do
  target="${enc%.enc}"
  [ -f "$target" ] && openssl enc -aes-256-cbc -pbkdf2 -in "$target" -out "$enc" -pass pass:"$PASSWORD" && echo "Hidden: $enc"
done
