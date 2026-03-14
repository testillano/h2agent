#!/bin/bash
# Generate a markdown benchmark report from monitor data and launcher output.
#
# Usage: report.sh <monitor_dir> <report_file> [launcher_output_file]
#
# Reads:
#   <monitor_dir>/cpu_mem.csv
#   <monitor_dir>/prom_before.txt
#   <monitor_dir>/prom_after.txt
#   <monitor_dir>/metadata.env      (TEST_NAME, ST_LAUNCHER, etc.)
#   [launcher_output_file]          (h2load raw output)
#
# Produces:
#   <report_file>                   (markdown report)

SCR_DIR=$(readlink -f "$(dirname "$0")")
MONITOR_DIR=$1
REPORT_FILE=$2
LAUNCHER_OUTPUT=$3

[ -z "$MONITOR_DIR" -o -z "$REPORT_FILE" ] && echo "Usage: $0 <monitor_dir> <report_file> [launcher_output]" && exit 1

# Load metadata
[ -f "${MONITOR_DIR}/metadata.env" ] && source "${MONITOR_DIR}/metadata.env"

{
  echo "# Benchmark Report"
  echo
  echo "Date: $(date '+%Y-%m-%d %H:%M:%S')"
  echo
  echo "## Test Profile"
  echo
  echo "| Parameter | Value |"
  echo "|---|---|"
  echo "| Profile | ${TEST_NAME:-unknown} |"
  echo "| Description | ${TEST_DESC:-} |"
  echo "| Launcher | ${ST_LAUNCHER:-unknown} |"
  echo "| Method | ${ST_REQUEST_METHOD:-} |"
  echo "| URI | ${ST_REQUEST_URI:-} |"
  [ -n "${H2CLIENT__CPS}" ] && echo "| CPS | ${H2CLIENT__CPS} |"
  [ -n "${H2CLIENT__ITERATIONS}" ] && echo "| Iterations | ${H2CLIENT__ITERATIONS} |"
  [ -n "${H2LOAD__ITERATIONS}" ] && echo "| Iterations | ${H2LOAD__ITERATIONS} |"
  [ -n "${H2LOAD__CLIENTS}" ] && echo "| Clients | ${H2LOAD__CLIENTS} |"
  [ -n "${H2LOAD__CONCURRENT_STREAMS}" ] && echo "| Concurrent streams | ${H2LOAD__CONCURRENT_STREAMS} |"
  [ -n "${H2AGENT__RESPONSE_DELAY_MS}" -a "${H2AGENT__RESPONSE_DELAY_MS}" != "0" ] && echo "| Response delay (ms) | ${H2AGENT__RESPONSE_DELAY_MS} |"
  [ -n "${ELAPSED_MS}" ] && echo "| Elapsed (ms) | ${ELAPSED_MS} |"
  [ -n "${ACTUAL_CPS}" ] && echo "| Actual CPS | ${ACTUAL_CPS} |"

  # CPU & Memory stats
  if [ -f "${MONITOR_DIR}/cpu_mem.csv" ]; then
    local_stats=$(awk -F, 'NR>1 && $2!="" {
      cpu+=$2; mem+=$3; n++;
      if(n==1||$2+0>cpu_max) cpu_max=$2+0;
      if(n==1||$2+0<cpu_min) cpu_min=$2+0;
      if(n==1||$3+0>mem_max) mem_max=$3+0;
      if(n==1||$3+0<mem_min) mem_min=$3+0;
    } END {
      if(n>0) printf "%.1f|%.1f|%.1f|%d|%d|%d|%d\n",cpu_min,cpu/n,cpu_max,mem_min,mem/n,mem_max,n
    }' "${MONITOR_DIR}/cpu_mem.csv")

    if [ -n "$local_stats" ]; then
      IFS='|' read cpu_min cpu_avg cpu_max mem_min mem_avg mem_max samples <<< "$local_stats"
      echo
      echo "## Resource Usage (${samples} samples)"
      echo
      echo "| Metric | Min | Avg | Max |"
      echo "|---|---|---|---|"
      echo "| CPU (%) | ${cpu_min} | ${cpu_avg} | ${cpu_max} |"
      echo "| RSS (KB) | ${mem_min} | ${mem_avg} | ${mem_max} |"
    fi
  fi

  # h2load latency (server mode)
  if [ -n "$LAUNCHER_OUTPUT" -a -f "$LAUNCHER_OUTPUT" ]; then
    h2load_latency=$(awk '/^time for request:|^time for connect:|^time to 1st byte:|^req\/s/ {
      label=$0; sub(/:.*/, ":", label); gsub(/^ +| +$/, "", label)
      # extract values after the colon
      rest=$0; sub(/^[^:]+:/, "", rest)
      n=split(rest, v, /[[:space:]]+/)
      if (n >= 5) printf "%s|%s|%s|%s|%s|%s\n", label, v[2], v[3], v[4], v[5], v[6]
    }' "$LAUNCHER_OUTPUT")

    if [ -n "$h2load_latency" ]; then
      echo
      echo "## h2load Latency"
      echo
      echo "| Metric | Min | Max | Mean | SD | ±SD% |"
      echo "|---|---|---|---|---|---|"
      while IFS='|' read label min max mean sd pct; do
        echo "| ${label} | ${min} | ${max} | ${mean} | ${sd} | ${pct} |"
      done <<< "$h2load_latency"
    fi
  fi

  # Prometheus stats (counters + latency)
  if [ -f "${MONITOR_DIR}/prom_before.txt" -a -f "${MONITOR_DIR}/prom_after.txt" ]; then
    prom_json=$(python3 "${SCR_DIR}/collect-prometheus-stats.py" "${MONITOR_DIR}/prom_before.txt" "${MONITOR_DIR}/prom_after.txt" --json 2>/dev/null)

    if [ -n "$prom_json" ]; then
      # Counters section (client mode)
      sent=$(echo "$prom_json" | jq -r '.counters.sent')
      if [ "${sent:-0}" != "0" ]; then
        echo
        echo "## Prometheus Counters"
        echo
        echo "| Metric | Value |"
        echo "|---|---|"
        echo "| Sent | ${sent} |"
        unsent=$(echo "$prom_json" | jq -r '.counters.unsent')
        [ "${unsent:-0}" != "0" ] && echo "| Unsent | ${unsent} |"
        timedout=$(echo "$prom_json" | jq -r '.counters.timedout')
        [ "${timedout:-0}" != "0" ] && echo "| Timedout | ${timedout} |"
        echo "$prom_json" | jq -r '.counters.status_classes | to_entries[] | select(.value > 0) | "| \(.key) | \(.value) |"'
      fi

      # Latency section (both modes)
      has_latency=$(echo "$prom_json" | jq -r '.latency | length')
      if [ "${has_latency:-0}" != "0" ]; then
        echo
        echo "## Prometheus Latency"
        echo
        echo "| Side | Label | Count | Avg | p50 | p90 | p99 |"
        echo "|---|---|---|---|---|---|---|"
        for side in server client; do
          echo "$prom_json" | jq -r --arg s "$side" '
            .latency[$s] // {} | to_entries[] |
            "| \($s) | \(.key) | \(.value.count) | \(.value.avg_ms | . * 1000 | round / 1000)ms | \(.value.p50_ms | . * 1000 | round / 1000)ms | \(.value.p90_ms | . * 1000 | round / 1000)ms | \(.value.p99_ms | . * 1000 | round / 1000)ms |"
          '
        done
      fi
    fi
  fi

  # Launcher raw output
  if [ -n "$LAUNCHER_OUTPUT" -a -f "$LAUNCHER_OUTPUT" ]; then
    echo
    echo "## Launcher Output"
    echo
    echo '```'
    cat "$LAUNCHER_OUTPUT"
    echo '```'
  fi

} > "$REPORT_FILE"

echo "Report generated: ${REPORT_FILE}"
