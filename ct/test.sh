#!/bin/bash

# Prepend variables:
#
#   XTRA_HELM_SETS: setters for helm install execution, for example:
#                   XTRA_HELM_SETS="--set h2agent.h2agent.cl.admin_port=8075 --set h2agent.h2agent.service.port=8075" ct/test.sh

#############
# VARIABLES #
#############

REPO_DIR="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$REPO_DIR" ] && { echo "You must execute under a valid git repository !" ; exit 1 ; }

CHART_NAME=ct-h2agent
NAMESPACE="ns-${CHART_NAME}"
HELM_CHART="helm/${CHART_NAME}"

#############
# FUNCTIONS #
#############
# $1: namespace; $2: optional prefix app filter
get_pod() {
  local filter="-o=custom-columns=:.metadata.name"
  [ -n "$2" ] && filter+=" -l app=${2}"

  kubectl --namespace=${1} get pod --no-headers ${filter}
  return $?
}

#############
# EXECUTION #
#############

cd "${REPO_DIR}"

echo
echo "===================================="
echo "Chart name:   ${CHART_NAME}"
echo "Namespace:    ${NAMESPACE}"
echo "===================================="

echo -e "\nCleaning up ..."
helm delete "${CHART_NAME}" -n "${NAMESPACE}" &>/dev/null

echo -e "\nUpdating helm chart dependencies ..."
helm dep update "${HELM_CHART}" &>/dev/null || { echo "Error !"; exit 1 ; }

echo -e "\nDeploying chart ..."
kubectl create namespace "${NAMESPACE}" &>/dev/null
helm install "${CHART_NAME}" "${HELM_CHART}" -n "${NAMESPACE}" ${XTRA_HELM_SETS} --wait || { echo "Error !"; exit 1 ; }

echo -e "\nExecuting tests ..."
test_pod="$(get_pod "${NAMESPACE}" | grep "^test")"
kubectl exec -it "${test_pod}" -n "${NAMESPACE}" -- pytest

