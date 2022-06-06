#!/bin/bash
# Repeat last execution
cd $(dirname $0)
[ ! -f last ] && echo "Missing last execution report !" && exit 1
source <(sed -n '/^---/,/^---/p' last | grep -v ^---)
