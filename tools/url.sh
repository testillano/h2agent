#!/bin/bash
# requires python module 'parse' (pip3 install parse)

#############
# FUNCTIONS #
#############
usage() {
  echo "Usage: $0 <--encode|--decode> <url>"
  exit 1
}

urlencode() {
  python3 -c "import urllib.parse as ul; print(ul.quote('$1'))"
}

urldecode() {
  python3 -c "import urllib.parse as ul; print(ul.unquote('$1'))"
}

#############
# EXECUTION #
#############
echo
[ -z "$2" ] && usage
[ "$1" = "--encode" ] && echo "Encoded URL: $(urlencode "$2")" && exit 0
[ "$1" = "--decode" ] && echo "Decoded URL: $(urldecode "$2")" && exit 0
usage
