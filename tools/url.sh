#!/bin/bash
# requires jq (encode)

#############
# FUNCTIONS #
#############
usage() {
  echo "Usage: $0 <--encode|--decode> <url>"
  exit 1
}

urlencode() {
  jq -rn --arg s "$1" '$s|@uri'
}

urldecode() {
  # Only valid %XX sequences are decoded; backslashes stay literal
  printf '%b\n' "$(sed -E 's/\\/\\\\/g; s/%([0-9A-Fa-f]{2})/\\x\1/g' <<< "$1")"
}

#############
# EXECUTION #
#############
echo
[ -z "$2" ] && usage
[ "$1" = "--encode" ] && echo "Encoded URL: $(urlencode "$2")" && exit 0
[ "$1" = "--decode" ] && echo "Decoded URL: $(urldecode "$2")" && exit 0
usage
