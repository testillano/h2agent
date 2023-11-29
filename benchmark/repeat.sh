#!/bin/bash
[ "$1" = "-h" -o "$1" = "--help" ] && echo -e "\nUsage: $0 [report file path: last by default]\n" && exit 0
SCR_DIR=$(dirname "$(readlink -f $0)")
report=${1:-"${SCR_DIR}/last"}
[ ! -f "${report}" ] && echo -e "\nError: file '${report}' not found !\n" && exit 1
echo -e "\nRe-executing '${report}' report ...\n"
[ ! -f ${report} ] && echo "Missing last execution report !" && exit 1
source <(sed -n '/^---/,/^---/p' "${report}" | grep -v ^---)
