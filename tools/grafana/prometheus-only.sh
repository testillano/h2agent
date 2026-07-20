#!/bin/bash
#
# Launch only Prometheus (no Grafana) for remote scraping.
# Useful when Grafana is already running elsewhere (e.g., a shared lab).
#
# Usage:
#   ./prometheus-only.sh [port1] [port2] ...
#
#   Provide all h2agent metrics ports to scrape (default: 8080).
#   A single Prometheus instance scrapes all targets.
#
# Examples:
#   ./prometheus-only.sh 8080         # one h2agent instance
#   ./prometheus-only.sh 8080 8081    # two instances (e.g., client + server)
#
# Prometheus will be available at http://localhost:<PROMETHEUS_PORT>
# Add it as datasource in your remote Grafana.
#
# Environment:
#   PROMETHEUS_IMAGE  Docker image (default: prom/prometheus:latest)
#   PROMETHEUS_PORT   Prometheus listen port (default: 9090)
#

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

PROM_PORT=${PROMETHEUS_PORT:-9090}

# Default ports
PORTS=(${@:-8080})

# Generate prometheus config with targets
TARGETS=$(printf ', "localhost:%s"' "${PORTS[@]}")
TARGETS="[${TARGETS:2}]"  # remove leading comma+space

PROM_CONFIG="${SCRIPT_DIR}/prometheus/prometheus-only.yaml"
cat > "${PROM_CONFIG}" << EOF
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

echo "Prometheus config: ${PROM_CONFIG}"
echo "Scraping targets: ${TARGETS}"
echo "Prometheus UI: http://localhost:${PROM_PORT}"
echo

docker rm -f prom-standalone 2>/dev/null || true
docker run --name prom-standalone -d --network=host \
  -v "${SCRIPT_DIR}/prometheus/prometheus-only.yaml:/etc/prometheus/prometheus.yml:ro" \
  -v prom_standalone_data:/prometheus \
  ${PROMETHEUS_IMAGE:-prom/prometheus:latest} \
  --config.file=/etc/prometheus/prometheus.yml \
  --storage.tsdb.path=/prometheus \
  --web.listen-address=:${PROM_PORT} \
  --web.console.libraries=/usr/share/prometheus/console_libraries \
  --web.console.templates=/usr/share/prometheus/consoles

# Health check
sleep 2
if curl -sf "http://localhost:${PROM_PORT}/-/healthy" >/dev/null 2>&1; then
  echo "Prometheus started OK. Add http://$(hostname -f):${PROM_PORT} as datasource in Grafana."
else
  echo "ERROR: Prometheus failed to start. Check: docker logs prom-standalone"
  exit 1
fi
