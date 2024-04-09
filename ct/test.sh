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

# Prepends:
s_XTRA_HELM_SETS="-"
[ -n "${XTRA_HELM_SETS}" ] && s_XTRA_HELM_SETS=${XTRA_HELM_SETS}
s_SKIP_HELM_DEPS=false
[ -n "${SKIP_HELM_DEPS}" ] && s_SKIP_HELM_DEPS=true
s_TEST_PROMPT=false
[ -n "${TEST_PROMPT}" ] && s_TEST_PROMPT=true

#############
# FUNCTIONS #
#############
usage() {
  cat << EOF

Usage: $0 [-h|--help] [action: [all]|deploy|test|hints] [ pytest extra options ]

       Positional options:

       -h|--help:      this help

       action:         Deploy, tests and hints shown by default, but also
                       just 'deploy', 'test' or 'hints' could be requested.

       pytest options: extra options passed to 'pytest' executable.


       Prepend variables:

       XTRA_HELM_SETS: additional setters for helm install execution.
       SKIP_HELM_DEPS: non-empty value skip helm dependencies update.
       TAG:            h2agent and ct-h2agent images tag for deployment
                       (latest by default).
       TEST_PROMPT:    non-empty value enables interactive testing.

       Examples:

       XTRA_HELM_SETS="--set h2agent.service.admin_port=8075 --set h2agent.service.traffic_port=8001" $0
       XTRA_HELM_SETS="--set h2agent.h2agent_cl.verbose.enabled=false" $0
       XTRA_HELM_SETS="--set h2agent.h2agent_cl.metrics.enabled=false" $0
       TAG=test1 $0 deploy
       $0 hints
       $0 test ${ALL_TESTS[$((RANDOM%${#ALL_TESTS[@]}))]} -k test_001 # pytest arguments
       TEST_PROMPT=true $0 # a menu with all the available tests is shown
EOF
}

# $1: namespace; $2: optional prefix app filter
get_pod() {
  #local filter="-o=custom-columns=:.metadata.name --field-selector=status.phase=Running"
  # There is a bug in kubectl: field selector status.phase is Running also for teminating pods
  local filter=
  [ -n "$2" ] && filter+=" -l app.kubernetes.io/name=${2}"

  # shellcheck disable=SC2086
  kubectl --namespace "$1" get pod --no-headers ${filter} | awk '{ if ($3 == "Running") print $1 }'
  return $?
}

# $1: test pod; $2-@: pytest arguments
do_test() {
  local test_pod=$1
  shift
  # shellcheck disable=SC2068
  kubectl exec -it "${test_pod}" -c test -n "${NAMESPACE}" -- sh -c "source /venv/bin/activate && pytest $@"
}

#############
# EXECUTION #
#############

# shellcheck disable=SC2164
cd "${REPO_DIR}"

# shellcheck disable=SC2166
[ "$1" = "-h" -o "$1" = "--help" ] && usage && exit 0

# Action
action=$1
s_action=${action:-"Deploy and test"}
DEPLOY=true
TEST=true
if [ -n "$1" ]
then
  case ${action} in
    all) s_action="Deploy and test" ;;
    deploy) TEST= ;;
    test) DEPLOY= ;;
    hints) DEPLOY= ; TEST= ;;
    *) echo "ERROR: invalid action (allowed: all, deploy, test)" && exit 1
  esac
fi

echo
echo "==============================="
echo "Component test procedure script"
echo "==============================="
echo
echo "(-h|--help for more information)"
echo
echo "Chart name:       ${CHART_NAME}"
echo "Namespace:        ${NAMESPACE}"
echo
echo "XTRA_HELM_SETS:   ${s_XTRA_HELM_SETS}"
echo "SKIP_HELM_DEPS:   ${s_SKIP_HELM_DEPS}"
echo "TAG:              ${TAG}"
echo "TEST_PROMPT:      ${s_TEST_PROMPT}"
shift
[ $# -gt 0 -a -n "${TEST}" ] && echo "Pytest arguments: $*"
echo

RC=0

if [ -n "${DEPLOY}" ]
then
  echo -e "\nCleaning up ..."
  helm delete "${CHART_NAME}" -n "${NAMESPACE}" &>/dev/null
  kubectl delete namespace ${NAMESPACE} &>/dev/null
  #echo "Press ENTER to continue, CTRL-C to abort ..."
  #read -r dummy

  echo -e "\nPreparing to deploy chart '${CHART_NAME}' ..."
  # just in case, some failed deployment exists:
  helm delete "${CHART_NAME}" -n "${NAMESPACE}" &>/dev/null
  kubectl delete namespace ${NAMESPACE} &>/dev/null

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
     --set h2agent.h2agent_cl.traffic_server_api_name="app" \
     --set h2agent.h2agent_cl.traffic_server_api_version="v1" \
     --set test.image.tag="${TAG}" \
     --set h2agent.image.tag="${TAG}" \
     ${XTRA_HELM_SETS} || { echo "Error !"; exit 1 ; }
#     --set h2agent_image.tag="${TAG}" \
else
  echo -e "\nDeployment skipped !"
fi

if [ -n "${TEST}" ]
then
  echo -e "\nExecuting tests ..."
  test_pod="$(get_pod "${NAMESPACE}" ct-h2agent)"
  [ -z "${test_pod}" ] && echo "Missing target pod for test" && exit 1

  if [ -n "${TEST_PROMPT}" ]
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
    RC=$?
  fi
else
  echo -e "\nTests skipped !"
fi

# Final hints:
deployed=$(helm list -q --deployed -n "${NAMESPACE}" | grep -w "${CHART_NAME}")
if [ ${RC} -eq 0 ]
then
  if [ -n "${deployed}" ]
  then
    SCRAPE_PORT=$(kubectl get service -n ${NAMESPACE} -l app.kubernetes.io/name=h2agent -o=jsonpath='{.items[0].spec.ports[?(@.name=="http-metrics")].port}')
    ADMIN_PORT=$(kubectl get service -n ${NAMESPACE} -l app.kubernetes.io/name=h2agent -o=jsonpath='{.items[0].spec.ports[?(@.name=="http2-admin")].port}')
    TRAFFIC_PORT=$(kubectl get service -n ${NAMESPACE} -l app.kubernetes.io/name=h2agent -o=jsonpath='{.items[0].spec.ports[?(@.name=="http2-traffic")].port}')
    h2agent_pod="$(get_pod "${NAMESPACE}" h2agent)"
    [ -z "${ADMIN_PORT}" -o -z "${TRAFFIC_PORT}" -o -z "${h2agent_pod}" ] && exit 0 # ignore hints (scrape port could be missing)
    cat << EOF

You may want to start a proxy to minikube Cluster IP, in order to use native utilities like:
   $ benchmark/start.sh -y    # traffic load
   $ source tools/helpers.src # troubleshooting
   etc.

Then, just execute these port-forward commands:

$([ -n "${SCRAPE_PORT}" ] && echo "kubectl port-forward ${h2agent_pod} ${SCRAPE_PORT}:${SCRAPE_PORT} -n ${NAMESPACE} &")
kubectl port-forward ${h2agent_pod} ${ADMIN_PORT}:${ADMIN_PORT} -n ${NAMESPACE} &
kubectl port-forward ${h2agent_pod} ${TRAFFIC_PORT}:${TRAFFIC_PORT} -n ${NAMESPACE} &

EOF
  else
    echo -e "\nNo deployment detected"
  fi
fi

echo
echo "[RC=${RC}]"
echo
exit ${RC}

