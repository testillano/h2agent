#!/bin/bash

#############
# VARIABLES #
#############
REPO_DIR="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$REPO_DIR" ] && { echo "You must execute under a valid git repository !" ; exit 1 ; }

CHART_NAME=h2agent-grafana
NAMESPACE="ns-${CHART_NAME}"
SCRAPE_PORT=8080
PM_PORT=9090
GF_PORT=3000

#############
# EXECUTION #
#############
echo
echo "Confirm namespace [${NAMESPACE}]:"
read opt
[ -n "${opt}" ] && NAMESPACE=${opt}

# Cleanups
echo "Cleaning up ..."
helm delete -n ${NAMESPACE} ${CHART_NAME} prometheus grafana &>/dev/null
kubectl delete namespace ${NAMESPACE} &>/dev/null
#echo "Press ENTER to continue, CTRL-C to abort ..."
#read -r dummy

# Deploy h2agent
kubectl create namespace ${NAMESPACE} &>/dev/null
helm install -n ${NAMESPACE} ${CHART_NAME} ${REPO_DIR}/helm/h2agent \
  --set h2agent_cl.prometheus_response_delay_seconds_histogram_boundaries="0.001 0.002 0.005 0.01 0.015 0.02 0.05 0.1" \
  --set h2agent_cl.prometheus_message_size_bytes_histogram_boundaries=""

# Add prometheus repository
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts

# Install prometheus
helm delete -n ${NAMESPACE} prometheus &>/dev/null
cat << EOF > /tmp/values.yaml
serverFiles:
  prometheus.yml:
    scrape_configs:
      - job_name: h2agent
        static_configs:
          - targets:
            - h2agent:8080
EOF
helm install -n ${NAMESPACE} prometheus prometheus-community/prometheus -f /tmp/values.yaml
rm /tmp/values.yaml
kubectl -n ${NAMESPACE} expose service prometheus-server --type=NodePort --target-port=${PM_PORT} --name=prometheus-server-np

# Add grafana repository
helm repo add grafana https://grafana.github.io/helm-charts

# Install grafana
helm delete -n ${NAMESPACE} grafana &>/dev/null
helm install -n ${NAMESPACE} grafana grafana/grafana --set adminPassword="admin12345"
kubectl -n ${NAMESPACE} expose service grafana --type=NodePort --target-port=${GF_PORT} --name=grafana-np

echo
H2A_POD=$(kubectl -n ${NAMESPACE} get pods -l "app.kubernetes.io/name=h2agent" -o jsonpath="{.items[0].metadata.name}")
echo "Type this to scrape metrics directly:"
echo "   kubectl -n ${NAMESPACE} port-forward ${H2A_POD} ${SCRAPE_PORT}:${SCRAPE_PORT} &"
echo "   firefox http://localhost:${SCRAPE_PORT}/metrics"
echo
echo "Type this to browse prometheus:"
echo "   minikube service prometheus-server-np -n ${NAMESPACE}"
echo
GF_USER=$(kubectl -n ${NAMESPACE} get secret grafana -o jsonpath="{.data.admin-user}" | base64 --decode)
GF_PASSWORD=$(kubectl -n ${NAMESPACE} get secret grafana -o jsonpath="{.data.admin-password}" | base64 --decode)
echo "Type this to browse grafana (${GF_USER}/${GF_PASSWORD}):"
echo "   minikube service grafana-np -n ${NAMESPACE}"
echo "   Configure prometheus as data source from URL 'http://prometheus-server:80'"
echo

