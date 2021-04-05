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
       NO_DEPLOY:      non-empty value skips deployment stage.
       TAG:            h2agent and ct-h2agent images tag for deployment (latest by default).
                       When NO_DEPLOY is also provided, pod images will be updated.

       Examples:

       XTRA_HELM_SETS="--set h2agent.h2agent.cl.admin_port=8075 --set h2agent.h2agent.service.port=8075" $0
       NO_DEPLOY=true $0
       $0 -n 4
       NO_DEPLOY=true TAG=test1 $0
EOF
}

# $1: namespace; $2: optional prefix app filter
get_pod() {
  #local filter="-o=custom-columns=:.metadata.name --field-selector=status.phase=Running"
  # There is a bug in kubectl: field selector status.phase is Running also for teminating pods
  local filter=
  [ -n "$2" ] && filter+=" -l app.kubernetes.io/name=${2}"

  kubectl --namespace=${1} get pod --no-headers ${filter} | awk '{ if ($3 == "Running") print $1 }'
  return $?
}

#############
# EXECUTION #
#############

cd "${REPO_DIR}"

[ "$1" = "-h" -o "$1" = "--help" ] && usage && exit 0

echo
echo "===================================="
echo "Chart name:     ${CHART_NAME}"
echo "Namespace:      ${NAMESPACE}"
echo "TAG:            ${TAG}"
[ -n "${NO_DEPLOY}" ] && echo "NO_DEPLOY:      selected"
[ -n "${XTRA_HELM_SETS}" ] && echo "XTRA_HELM_SETS: ${XTRA_HELM_SETS}"
echo "===================================="
echo "(-h|--help for more information)"
echo

if [ -n "${NO_DEPLOY}" ]
then
  if [ -n "${TAG}" ]
  then
    echo -e "\nWaiting for upgrade to tag '${TAG}' ..."
    kubectl set image "deployment/h2agent" -n "${NAMESPACE}" h2agent=testillano/h2agent:"${TAG}" &>/dev/null
    kubectl set image "deployment/test" -n "${NAMESPACE}" test=testillano/ct-h2agent:"${TAG}" &>/dev/null
    test_pod="$(get_pod "${NAMESPACE}" ct-h2agent)"
    h2agent_pod="$(get_pod "${NAMESPACE}" h2agent)"
    [ -z "${test_pod}" -o -z "${h2agent_pod}" ] && echo "Missing target pods to upgrade" && exit 1

    # Check 10 times, during 1 minute (every 6 seconds):
    attempts=0
    until kubectl rollout status deployment/test -n "${NAMESPACE}" &>/dev/null || [ ${attempts} -eq 10 ]; do
      echo -n .
      sleep 6
      attempts=$((attempts + 1))
    done
  fi
else
  echo -e "\nCleaning up ..."
  helm delete "${CHART_NAME}" -n "${NAMESPACE}" &>/dev/null

  echo -e "\nUpdating helm chart dependencies ..."
  helm dep update "${HELM_CHART}" &>/dev/null || { echo "Error !"; exit 1 ; }

  echo -e "\nDeploying chart ..."
  kubectl create namespace "${NAMESPACE}" &>/dev/null
  helm install "${CHART_NAME}" "${HELM_CHART}" -n "${NAMESPACE}" --wait \
     --set test.image.tag="${TAG}" \
     --set h2agent.image.tag="${TAG}" \
     ${XTRA_HELM_SETS} || { echo "Error !"; exit 1 ; }
fi

echo -e "\nExecuting tests ..."
test_pod="$(get_pod "${NAMESPACE}" ct-h2agent)"
[ -z "${test_pod}" ] && echo "Missing target pod to test" && exit 1
kubectl exec -it "${test_pod}" -n "${NAMESPACE}" -- pytest $@

