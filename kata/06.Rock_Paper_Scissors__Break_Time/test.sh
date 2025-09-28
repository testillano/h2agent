#!/bin/bash

#############
# FUNCTIONS #
#############
# $1: user1; $2: choice1; $3: user2; $4: choice2
result() {
  local user1="$1"
  local choice1="$2"
  local user2="$3"
  local choice2="$4"

  local why="Both chose '${choice1}'"
  local winner="the winner is: ${user1}"

  [ "${choice1}" = "rock" -a "${choice2}" = "paper" ] && winner="the winner is: ${user2}"
  [ "${choice1}" = "paper" -a "${choice2}" = "scissors" ] && winner="the winner is: ${user2}"
  [ "${choice1}" = "scissors" -a "${choice2}" = "rock" ] && winner="the winner is: ${user2}"
  [ "${choice1}" = "${choice2}" ] && winner="players TIE !"

  echo "${choice1} ${choice2}" | grep -w rock | grep -qw paper
  [ $? -eq 0 ] && why="Paper covers rock"

  echo "${choice1} ${choice2}" | grep -w rock | grep -qw scissors
  [ $? -eq 0 ] && why="Rock crushes scissors"

  echo "${choice1} ${choice2}" | grep -w scissors | grep -qw paper
  [ $? -eq 0 ] && why="Scissors cuts paper"

  echo "${why} => ${winner}"
}

#############
# EXECUTION #
#############
cd $(dirname $0)
source ../../tools/common.src

h2agent_server_configuration

if [ -z "${NON_INTERACT}" ] # user playing
then
  while true
  do
    echo
    title "Select your choice:"
    echo "1. ROCK"
    echo "2. PAPER"
    echo "3. SCISSORS"
    echo
    echo "0. Exit"
    echo
    read choice
    while [ -z "${choice}" ]; do read choice; done
    CHOICE=
    EXPECTED_RESPONSE=
    case ${choice} in
      1) CHOICE=rock ; EXPECTED_RESPONSE="\"scissors\"" ;;
      2) CHOICE=paper ; EXPECTED_RESPONSE="\"rock"\" ;;
      3) CHOICE=scissors ; EXPECTED_RESPONSE="\"paper"\" ;;
      0) exit 0 ;;
    esac
    [ -z "${CHOICE}" ] && echo "Invalid selection, try again !" && continue

    EXPECTED_STATUS_CODES=200
    test_query "H2agent playing against you (you win if response is OK) ..." GET http://${H2AGENT_TRAFFIC_ENDPOINT}/rock%20paper%20or%20scissors
    CHOICE_H2AGENT=$(tail -1 ${TMPDIR}/cmd.out | tr -d '"')

    title "$(result "You" ${CHOICE} "H2agent" ${CHOICE_H2AGENT})"
  done
else
  EXPECTED_STATUS_CODES=200
  test_query "H2agent playing for Player 1 ..." GET http://${H2AGENT_TRAFFIC_ENDPOINT}/rock%20paper%20or%20scissors || exit 1
  CHOICE_PLAYER1=$(tail -1 ${TMPDIR}/cmd.out | tr -d '"')
  title "${CHOICE_PLAYER1}" "" "*"

  EXPECTED_STATUS_CODES=200
  test_query "H2agent playing for Player 2 ..." GET http://${H2AGENT_TRAFFIC_ENDPOINT}/rock%20paper%20or%20scissors || exit 1
  CHOICE_PLAYER2=$(tail -1 ${TMPDIR}/cmd.out | tr -d '"')
  title "${CHOICE_PLAYER2}" "" "*"

  WINNER="the winner is: Player 1"
  [ "${CHOICE_PLAYER1}" = "rock" -a "${CHOICE_PLAYER2}" = "paper" ] && WINNER="the winner is: Player 2"
  [ "${CHOICE_PLAYER1}" = "paper" -a "${CHOICE_PLAYER2}" = "scissors" ] && WINNER="the winner is: Player 2"
  [ "${CHOICE_PLAYER1}" = "scissors" -a "${CHOICE_PLAYER2}" = "rock" ] && WINNER="the winner is: Player 2"
  [ "${CHOICE_PLAYER1}" = "${CHOICE_PLAYER2}" ] && WINNER="players TIE !" && why="Both chose '${CHOICE_PLAYER1}'"


  echo
  title "$(result "Player 1" ${CHOICE_PLAYER1} "Player 2" ${CHOICE_PLAYER2})"
fi

