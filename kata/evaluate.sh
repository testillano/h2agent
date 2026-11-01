#!/bin/bash
# Set NON_INTERACT=true on calling shell to disable interactivity, and VERBOSE=true
# to get the full output of every request (one line per step by default).
#
# The result of every exercise is kept across runs (STATUS_FILE, local file ignored
# by git) and shown in the menu: [ NOT EVALUATED ], [ PASSED ] or [ FAILED ].
# An exercise holding a '.game' file is a game: it is evaluated as any other one,
# but shown as [ PLAY ME ] until played, and [ PLAYED ] when passed. With the
# 'a'/'f' options games run non-interactively; select their number to play them.

#############
# VARIABLES #
#############
STATUS_FILE=.evaluation/status    # <exercise dir>|<PASSED|FAILED>|<date>|<seconds>

#############
# FUNCTIONS #
#############
game() {
  [ -f "$1/.game" ]
}

status_of() {
  local dir=$1
  local status=$(grep -F "${dir}|" ${STATUS_FILE} 2>/dev/null | tail -1 | cut -d'|' -f2)
  [ -z "${status}" ] && status="NOT EVALUATED"
  if game ${dir}
  then
    [ "${status}" = "PASSED" ] && status="PLAYED"
    [ "${status}" = "NOT EVALUATED" ] && status="PLAY ME"
  fi
  echo "${status}"
}

field_of() {
  local dir=$1 field=$2
  grep -F "${dir}|" ${STATUS_FILE} 2>/dev/null | tail -1 | cut -d'|' -f${field}
}

status_label() {
  case "$1" in
    PASSED) echo -e "${COLOR_green}[ PASSED ]${COLOR_reset}       " ;;
    FAILED) echo -e "${COLOR_red}[ FAILED ]${COLOR_reset}       " ;;
    PLAYED) echo -e "${COLOR_l_magenta}[ PLAYED ]${COLOR_reset}       " ;;
    "PLAY ME") echo -e "${COLOR_cyan}[ PLAY ME ]${COLOR_reset}      " ;;
    *)      echo -e "${COLOR_yellow}[ NOT EVALUATED ]${COLOR_reset}" ;;
  esac
}

save_status() {
  local dir=$1 verdict=$2 seconds=$3
  mkdir -p $(dirname ${STATUS_FILE})
  touch ${STATUS_FILE}
  grep -vF "${dir}|" ${STATUS_FILE} > ${STATUS_FILE}.tmp
  echo "${dir}|${verdict}|$(date '+%Y-%m-%d %H:%M:%S')|${seconds}" >> ${STATUS_FILE}.tmp
  mv ${STATUS_FILE}.tmp ${STATUS_FILE}
}

# Verdict as shown for the exercise (a passed game is PLAYED)
status_label_of() {
  game $1 && [ "$2" = "PASSED" ] && echo "PLAYED" || echo "$2"
}

# $1: exercise dir; $2: 'batch' when evaluating several exercises (a/f options): games
# run then in their non-interactive mode, so the evaluation does not stop on them
evaluate() {
  local dir=$1 mode=$2

  echo
  echo -e "${COLOR_magenta}${dir%/}${COLOR_reset}"
  local start=$(date +%s)
  if [ "${mode}" = "batch" ] && game ${dir}
  then
    NON_INTERACT=true ${dir}/test.sh
  else
    ${dir}/test.sh
  fi
  local rc=$?
  local seconds=$(( $(date +%s) - start ))
  if [ ${rc} -eq 0 ]
  then
    verdict=PASSED
    PASSED_COUNT=$((PASSED_COUNT+1))
  else
    verdict=FAILED
    RC=1
  fi
  TOTAL_COUNT=$((TOTAL_COUNT+1))
  echo "$(status_label "$(status_label_of ${dir} ${verdict})") (${seconds}s)"
  save_status ${dir} ${verdict} ${seconds}
  echo "$(status_label "$(status_label_of ${dir} ${verdict})") ${dir}" >> ${TMPDIR}/summary.txt
}

show_status() {
  local passed=0 n=0 dir status
  for dir in ${DIRECTORIES[@]}; do
    n=$((n+1))
    status=$(status_of ${dir})
    [ "${status}" = "PASSED" ] && passed=$((passed+1))
    echo "$(status_label "${status}") ${dir%/}" | sed -e "s/^/  /"
    [ "${status}" = "PLAYED" ] && passed=$((passed+1))
  done
  echo
  echo "  [Score: ${passed}/${n}]"
}

menu() {
  echo
  echo "Available exercises:"
  echo
  show_status
  echo
  echo "Select option (number: one exercise; a: all; f: failed and not evaluated; r: reset results; x: exit):"
  read opt
  [ "${opt}" = "x" ] && echo "Goodbye !" && exit 0
  [ -z "${opt}" ] && return 0 # just ENTER: show the menu again (never re-runs an evaluation)

  if [ "${opt}" = "r" ]
  then
    rm -f ${STATUS_FILE}
    return 0
  elif [ "${opt}" = "a" ]
  then
    for dir in ${DIRECTORIES[@]}; do evaluate ${dir} batch; done
  elif [ "${opt}" = "f" ]
  then
    for dir in ${DIRECTORIES[@]}; do case "$(status_of ${dir})" in PASSED|PLAYED) ;; *) evaluate ${dir} batch ;; esac; done
  else
    local re='^[0-9]+$'
    if ! [[ ${opt} =~ $re ]] ; then
      echo "Invalid option !" && return 1
    fi
    opt=$(echo ${opt} | sed -e 's/^[0]*//') # remove trailing zeroes
    indx=$((opt - 1))
    [ ${indx} -lt 0 -o ${indx} -ge ${#DIRECTORIES[@]} ] && echo "Invalid option !" && return 1
    evaluate ${DIRECTORIES[${indx}]}
  fi

  echo
  echo "======================="
  echo "KATA EVALUATION SUMMARY"
  echo "======================="
  echo
  echo "This run:"
  cat ${TMPDIR}/summary.txt | sed -e "s/^/  /"
  echo
  echo "All exercises:"
  show_status
  echo
  if [ ${TOTAL_COUNT} -eq 0 ]
  then
    echo "=> Nothing to evaluate: every exercise is already passed !"
  elif [ ${RC} -eq 0 ]
  then
    [ ${TOTAL_COUNT} -eq 1 ] && echo "=> Congratulations, you passed the exercise !" || echo "=> Congratulations, you passed the ${TOTAL_COUNT} evaluated exercises !"
  else
    [ ${TOTAL_COUNT} -eq 1 ] && echo "=> Exercise not passed, keep trying !" || echo "=> $((TOTAL_COUNT - PASSED_COUNT)) of ${TOTAL_COUNT} evaluated exercises not passed, keep trying !"
  fi
  local dir pending=0
  for dir in ${DIRECTORIES[@]}; do case "$(status_of ${dir})" in PASSED|PLAYED) ;; *) pending=$((pending+1)) ;; esac; done
  [ ${pending} -eq 0 ] && echo -e "${COLOR_l_green}=> KATA COMPLETED: all the exercises are passed !${COLOR_reset}"
  echo
  echo "[This run: ${PASSED_COUNT}/${TOTAL_COUNT}]"
}

#############
# EXECUTION #
#############
cd $(dirname $0)
echo
source ../tools/common.bash

title "H2agent kata"
echo "(remember: set NON_INTERACT=true on calling shell to disable interactivity, VERBOSE=true for the full output)"
[ -z "${VERBOSE}" ] && export COMPACT_OUTPUT=true
h2agent_check ${H2AGENT_ADMIN_ENDPOINT} ${H2AGENT_TRAFFIC_ENDPOINT} || exit 1

DIRECTORIES=( $(ls -d */) )

while true
do
  0> ${TMPDIR}/summary.txt
  RC=0
  PASSED_COUNT=0
  TOTAL_COUNT=0
  menu
done
