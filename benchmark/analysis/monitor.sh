#!/bin/bash
# Monitor h2agent process: samples CPU%, RSS and prometheus gauges periodically.
# Runs in background; caller must kill $MONITOR_PID when done.
#
# Usage: source monitor.sh
#        monitor_start <h2agent_pid> <output_dir> [prometheus_url] [interval_secs]
#        monitor_stop
#
# Produces:
#   <output_dir>/cpu_mem.csv    - timestamp_s,cpu_pct,rss_kb
#   <output_dir>/prom_before.txt - prometheus snapshot before
#   <output_dir>/prom_after.txt  - prometheus snapshot after (captured on stop)

MONITOR_PID=

monitor_start() {
  local pid=$1
  local outdir=$2
  local prom_url=${3:-}
  local interval=${4:-1}

  [ -z "$pid" -o -z "$outdir" ] && echo "monitor_start: requires <pid> <outdir>" && return 1

  # Snapshot prometheus before
  [ -n "$prom_url" ] && curl -sf "$prom_url" > "${outdir}/prom_before.txt" 2>/dev/null

  # CSV header
  echo "timestamp_s,cpu_pct,rss_kb" > "${outdir}/cpu_mem.csv"

  # Background sampler
  (
    while kill -0 "$pid" 2>/dev/null; do
      local ts=$(date +%s)
      local sample=$(ps -p "$pid" -o %cpu=,rss= 2>/dev/null)
      [ -n "$sample" ] && echo "${ts},$(echo $sample | tr ' ' ',')" >> "${outdir}/cpu_mem.csv"
      sleep "$interval"
    done
  ) &
  MONITOR_PID=$!
}

monitor_stop() {
  local outdir=$1
  local prom_url=${2:-}

  # Snapshot prometheus after
  [ -n "$prom_url" ] && curl -sf "$prom_url" > "${outdir}/prom_after.txt" 2>/dev/null

  # Kill sampler
  [ -n "$MONITOR_PID" ] && kill "$MONITOR_PID" 2>/dev/null && wait "$MONITOR_PID" 2>/dev/null
  MONITOR_PID=
}
