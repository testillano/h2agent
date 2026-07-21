#!/bin/bash
# Launch a standalone Prometheus container to scrape metrics ports.

#############
# VARIABLES #
#############

set -euo pipefail
PROM_IMAGE=${PROMETHEUS_IMAGE:-prom/prometheus:latest}
PROM_PORT=${PROMETHEUS_PORT:-9090}
PROM_NAME=${PROMETHEUS_NAME:-prom-standalone}
PROM_YAML=/tmp/prometheus-only.yaml

#############
# FUNCTIONS #
#############
usage() {
  cat << EOF
Usage: $(basename "$0") [options] [port1] [port2] ...

Launch a standalone Prometheus container to scrape metrics ports.
Useful when Grafana is already running elsewhere (e.g., a shared lab).
The script is generic: it can scrape any metrics port.

Arguments:
  port1, port2, ...   Metrics ports to scrape (default: 8080)

Options:
  -h, --help          Show this help

Environment:
  PROMETHEUS_IMAGE   Docker image (default: prom/prometheus:latest)
  PROMETHEUS_PORT    Prometheus listen port (default: 9090)
  PROMETHEUS_NAME:   Docker container name (default: prom-standalone)

Examples:
  $(basename "$0") 8081              # scrape h2agent client instance
  $(basename "$0") 8080 8081         # scrape h2agent server + client
  PROMETHEUS_PORT=9091 $(basename "$0")

EOF
}

#############
# EXECUTION #
#############
echo

# Parse args
PORTS=()
for arg in "$@"; do
  case "${arg}" in
    -h|--help) usage; exit 0 ;;
    *[!0-9]*) echo "ERROR: invalid port '${arg}' (must be numeric)"; exit 1 ;;
    *) PORTS+=("${arg}") ;;
  esac
done
[ ${#PORTS[@]} -eq 0 ] && PORTS=(8080)

# Check port is free
docker rm -f ${PROM_NAME} &>/dev/null || true # just in case it's me
if curl -sf "http://localhost:${PROM_PORT}/-/healthy" >/dev/null 2>&1; then
  echo "ERROR: port ${PROM_PORT} already in use (Prometheus or other service running)."
  echo "  Use PROMETHEUS_PORT=<other-port> or stop the existing service."
  exit 1
fi

# Generate targets
TARGETS=$(printf ', "localhost:%s"' "${PORTS[@]}")
TARGETS="[${TARGETS:2}]"

# Generate config
cat > "${PROM_YAML}" << EOF
global:
  scrape_interval: 5s
  external_labels:
    monitor: 'h2agent'

scrape_configs:
  - job_name: 'h2agent'
    static_configs:
      - targets: ${TARGETS}

  - job_name: 'prometheus'
    scrape_interval: 15s
    static_configs:
      - targets: ['localhost:${PROM_PORT}']
EOF

echo "Scraping targets: ${TARGETS}"
echo "Listen port: ${PROM_PORT}"
echo

# Launch container
docker rm -f ${PROM_NAME} &>/dev/null || true
docker run --name ${PROM_NAME} -d --network=host \
  -v "${PROM_YAML}:/etc/prometheus/prometheus.yml:ro" \
  -v prom_standalone_data:/prometheus \
  ${PROM_IMAGE} \
  --config.file=/etc/prometheus/prometheus.yml \
  --storage.tsdb.path=/prometheus \
  --web.listen-address=:${PROM_PORT} \
  --web.console.libraries=/usr/share/prometheus/console_libraries \
  --web.console.templates=/usr/share/prometheus/consoles >/dev/null

# Health check
sleep 2
if curl -sf "http://localhost:${PROM_PORT}/-/healthy" >/dev/null 2>&1; then
  echo "Prometheus started OK. Run 'docker rm -f ${PROM_NAME}' to stop the server."
  echo "Add http://$(hostname -f):${PROM_PORT} as datasource in Grafana GUI."
else
  echo "ERROR: Prometheus failed to start. Check: docker logs ${PROM_NAME}"
  exit 1
fi
echo

