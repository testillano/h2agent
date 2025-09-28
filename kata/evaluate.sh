#!/bin/bash
# Set NON_INTERACT=true on calling shell to disable interactivity

#############
# VARIABLES #
#############

#############
# FUNCTIONS #
#############
evaluate() {
  local dir=$1

  title "$(basename ${dir})" "${COLOR_magenta}"
  ${dir}/test.sh
  if [ $? -eq 0 ]
  then
    verdict=passed
    PASSED_COUNT=$((PASSED_COUNT+1))
  else
    verdict=FAILED
    RC=1
  fi
  TOTAL_COUNT=$((TOTAL_COUNT+1))
  echo "[${verdict}] ${dir}" >> ${TMPDIR}/summary.txt
}

menu() {
  echo
  title "Available exercises:"
  for dir in ${DIRECTORIES[@]}
  do
    echo ${dir} | tr '.' ' '
  done
  echo
  echo "Select option (x: exit; a: all) [${LAST_OPTION}]:"
  read opt
  [ "${opt}" = "x" ] && echo "Goodbye !" && exit 0
  [ -z "${opt}" ] && opt=${LAST_OPTION}

  if [ "${opt}" = "a" ]
  then
    for dir in ${DIRECTORIES[@]}; do evaluate ${dir}; done
  else
    local re='^[0-9]+$'
    if ! [[ ${opt} =~ $re ]] ; then
      echo "Invalid option! Press ENTER to continue ..." && read -r dummy && return 1
    fi
    opt=$(echo ${opt} | sed -e 's/^[0]*//') # remove trailing zeroes
    indx=$((opt - 1))
    [ ${indx} -lt 0 -o ${indx} -ge ${#DIRECTORIES[@]} ] && echo "Invalid option! Press ENTER to continue ..." && read -r dummy && return 1
    LAST_OPTION=$((indx + 1))
    evaluate ${DIRECTORIES[${indx}]}
  fi

  echo
  echo "======================="
  echo "KATA EVALUATION SUMMARY"
  echo "======================="
  echo
  cat ${TMPDIR}/summary.txt
  echo
  if [ ${RC} -eq 0 ]
  then
    echo "=> All OK !"
  else
    echo "=> KATA not passed !"
  fi
  echo
  echo "[Score: ${PASSED_COUNT}/${TOTAL_COUNT}]"
  echo
  echo "Press ENTER to continue ..."
  read -r dummy
}

#############
# EXECUTION #
#############
cd $(dirname $0)
echo
source ../tools/common.src

title "H2agent kata"
echo "(remember: set NON_INTERACT=true on calling shell to disable interactivity)"
h2agent_check ${H2AGENT_ADMIN_ENDPOINT} ${H2AGENT_TRAFFIC_ENDPOINT} || exit 1

DIRECTORIES=( $(ls -d */) )
LAST_OPTION=a # execute all

while true
do
  0> ${TMPDIR}/summary.txt
  RC=0
  PASSED_COUNT=0
  TOTAL_COUNT=0
  menu
done

