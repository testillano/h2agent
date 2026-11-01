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
MONITOR_TARGET_PID=

# Cumulative CPU ticks (utime+stime, all threads) of a process; empty if gone.
# Fields after the "(comm)" to be robust to spaces in the command name.
cpu_ticks() {
  local stat
  stat=$(cat "/proc/$1/stat" 2>/dev/null) || return 1
  echo "${stat##*) }" | awk '{ print $12 + $13 }'
}

monitor_start() {
  local pid=$1
  local outdir=$2
  local prom_url=${3:-}
  local interval=${4:-1}

  [ -z "$pid" -o -z "$outdir" ] && echo "monitor_start: requires <pid> <outdir>" && return 1

  # Snapshot prometheus before
  [ -n "$prom_url" ] && curl -sf "$prom_url" > "${outdir}/prom_before.txt" 2>/dev/null

  # CPU time baseline (consumed CPU over the run = stop - start; see monitor_stop)
  MONITOR_TARGET_PID=$pid
  cpu_ticks "$pid" > "${outdir}/cpu_ticks_start" 2>/dev/null

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

  # CPU time consumed during the run (ms): independent of throughput and of
  # how long the process lived before the run (unlike ps %cpu).
  if [ -n "$MONITOR_TARGET_PID" -a -s "${outdir}/cpu_ticks_start" ]; then
    local t0 t1 hz
    t0=$(cat "${outdir}/cpu_ticks_start"); t1=$(cpu_ticks "$MONITOR_TARGET_PID"); hz=$(getconf CLK_TCK)
    [ -n "$t1" ] && echo $(( (t1 - t0) * 1000 / hz )) > "${outdir}/cpu_time_ms"
  fi
  MONITOR_TARGET_PID=

  # Kill sampler
  [ -n "$MONITOR_PID" ] && kill "$MONITOR_PID" 2>/dev/null && wait "$MONITOR_PID" 2>/dev/null
  MONITOR_PID=
}
