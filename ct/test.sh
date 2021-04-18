#!/bin/bash

#############
# VARIABLES #
#############

REPO_DIR="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$REPO_DIR" ] && { echo "You must execute under a valid git repository !" ; exit 1 ; }

CHART_NAME=ct-h2agent
NAMESPACE="ns-${CHART_NAME}"
HELM_CHART="helm/${CHART_NAME}"
TAG=${TAG:-latest}
# shellcheck disable=SC2207,SC2012
ALL_TESTS=( $(ls "${REPO_DIR}"/ct/src/*/*_test.py | awk -F/ '{ print $(NF-1)"/"$NF }') )

#############
# FUNCTIONS #
#############
usage() {
  cat << EOF

Usage: $0 [-h|--help] [ pytest extra options ]

       pytest options: extra options passed to 'pytest' executable.

       -h|--help:      this help

       Prepend variables:

       XTRA_HELM_SETS: additional setters for helm install execution.
       SKIP_HELM_DEPS: non-empty value skip helm dependencies update.
       CLEAN:          non-empty value forces re-deployment. By default,
                       if a deployment exists, it will be reused.
       TAG:            h2agent and ct-h2agent images tag for deployment (latest by default).
                       Except when 'CLEAN' is provided, pod images will be updated.
       INTERACT:       non-empty value enables interactive testing (by default, all the
                       tests are executed).
       NO_TEST:        non-empty value skips test stage (only deploys).

       Examples:

       XTRA_HELM_SETS="--set h2agent.h2agent.cl.admin_port=8075 --set h2agent.h2agent.service.admin_port=8075" $0
       XTRA_HELM_SETS="--set h2agent.h2agent.cl.verbose.enabled=false" $0
       CLEAN=true $0 # deploys from scratch in case already deployed
       TAG=test1 $0
       $0 -n 4 # uses pytest-xdist
       $0 --workers auto --tests-per-worker auto # uses pytest-parallel (see more at https://github.com/browsertron/pytest-parallel#examples)
       $0 ${ALL_TESTS[$((RANDOM%${#ALL_TESTS[@]}))]} -k test_001 # pytest arguments
       INTERACT=true $ # a menu with all the available tests is shown
EOF
}

# $1: namespace; $2: optional prefix app filter
get_pod() {
  #local filter="-o=custom-columns=:.metadata.name --field-selector=status.phase=Running"
  # There is a bug in kubectl: field selector status.phase is Running also for teminating pods
  local filter=
  [ -n "$2" ] && filter+=" -l app.kubernetes.io/name=${2}"

  # shellcheck disable=SC2086
  kubectl --namespace="$1" get pod --no-headers ${filter} | awk '{ if ($3 == "Running") print $1 }'
  return $?
}

# $1: test pod; $2-@: pytest arguments
do_test() {
  local test_pod=$1
  shift
  # shellcheck disable=SC2068
  kubectl exec -it "${test_pod}" -c test -n "${NAMESPACE}" -- pytest $@
}

#############
# EXECUTION #
#############

# shellcheck disable=SC2164
cd "${REPO_DIR}"

# shellcheck disable=SC2166
[ "$1" = "-h" -o "$1" = "--help" ] && usage && exit 0

echo
echo "==============================="
echo "Component test procedure script"
echo "==============================="
echo
echo "(-h|--help for more information)"
echo
echo "Chart name:       ${CHART_NAME}"
echo "Namespace:        ${NAMESPACE}"
[ -n "${XTRA_HELM_SETS}" ] && echo "XTRA_HELM_SETS:   ${XTRA_HELM_SETS}"
[ -n "${CLEAN}" ] && echo "CLEAN:            selected"
echo "TAG:              ${TAG}"
[ -n "${INTERACT}" ] && echo "INTERACT:         selected"
[ -n "${NO_TEST}" ] && echo "NO_TEST:          selected"
[ $# -gt 0 ] && echo "Pytest arguments: $*"
echo

if [ -n "${CLEAN}" ]
then
  echo -e "\nCleaning up ..."
  helm delete "${CHART_NAME}" -n "${NAMESPACE}" &>/dev/null
fi

# Check deployment existence:
list=$(helm list -q --deployed -n "${NAMESPACE}" | grep -w "${CHART_NAME}")
if [ -n "${list}" ] # reuse
then
  echo -e "\nDetected deploment: reuse by default (provide 'CLEAN' to re-deploy)"
  echo -e "\nWaiting for upgrade to tag '${TAG}' ..."
  kubectl set image "deployment/h2agent" -n "${NAMESPACE}" h2agent=testillano/h2agent:"${TAG}" &>/dev/null
  kubectl set image "deployment/test" -n "${NAMESPACE}" test=testillano/ct-h2agent:"${TAG}" &>/dev/null
  test_pod="$(get_pod "${NAMESPACE}" ct-h2agent)"
  h2agent_pod="$(get_pod "${NAMESPACE}" h2agent)"
  # shellcheck disable=SC2166
  [ -z "${test_pod}" -o -z "${h2agent_pod}" ] && echo "Missing target pods to upgrade" && exit 1

  # Check 10 times, during 1 minute (every 6 seconds):
  attempts=0
  until kubectl rollout status deployment/test -n "${NAMESPACE}" &>/dev/null || [ ${attempts} -eq 10 ]; do
    echo -n .
    sleep 6
    attempts=$((attempts + 1))
  done
else
  echo -e "\nPreparing to deploy chart '${CHART_NAME}' ..."
  # just in case, some failed deployment exists:
  helm delete "${CHART_NAME}" -n "${NAMESPACE}" &>/dev/null

  echo -e "\nUpdating helm chart dependencies ..."
  if [ -n "${SKIP_HELM_DEPS}" ]
  then
    echo "Skipped !"
  else
    helm dep update "${HELM_CHART}" &>/dev/null || { echo "Error !"; exit 1 ; }
  fi

  echo -e "\nDeploying chart ..."
  kubectl create namespace "${NAMESPACE}" &>/dev/null
  # shellcheck disable=SC2086
  helm install "${CHART_NAME}" "${HELM_CHART}" -n "${NAMESPACE}" --wait \
     --set test.image.tag="${TAG}" \
     --set h2agent.image.tag="${TAG}" \
     ${XTRA_HELM_SETS} || { echo "Error !"; exit 1 ; }
fi

[ -n "${NO_TEST}" ] && echo -e "\nTests skipped !" && exit 0

echo -e "\nExecuting tests ..."
test_pod="$(get_pod "${NAMESPACE}" ct-h2agent)"
[ -z "${test_pod}" ] && echo "Missing target pod to test" && exit 1

if [ -n "${INTERACT}" ]
then
  while true
  do
    echo
    echo "Available test files:"
    echo
    count=1
    # shellcheck disable=SC2068
    for opt in ${ALL_TESTS[@]}
    do
      echo "${count}. ${opt}"
      count=$((count + 1))
    done
    echo
    echo "0. Exit"
    echo
    echo "Input option [0]:"
    read -r opt
    [ -z "${opt}" ] && opt=0
    [ ${opt} -eq 0 ] && exit 0
    test_file=${ALL_TESTS[$((opt - 1))]}
    [ -z "${test_file}" ] && echo "Invalid option !" && exit 1
    echo "Input k-filter (i.e. test_001) [omit]:"
    read -r opt
    k_filter="-k ${opt}"
    [ -z "${opt}" ] && k_filter=
    # shellcheck disable=SC2068,SC2086
    do_test "${test_pod}" $@ "${test_file}" ${k_filter}
  done
else
  # shellcheck disable=SC2068
  do_test "${test_pod}" $@
fi

