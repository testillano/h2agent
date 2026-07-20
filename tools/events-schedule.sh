#!/bin/bash
# tools/events-schedule.sh -- h2agent native event scheduler
#
# Usage:
#   tools/events-schedule.sh my-traffic.driver
#   tools/events-schedule.sh my-traffic.driver --from 1234
#   tools/events-schedule.sh my-traffic.driver --dry-run
#
# Driver labels encode actions directly:
#   cps:<provision-id>  -> client_provision_cps <id> <value> (supports <rate>#<rampup>)
#   trigger:<id>        -> client_provision_trigger <id>
#   vault:<key>         -> set vault variable <key> to <value>
#   trace               -> change logging level
#   snapshot            -> traffic_summary --save <value>
#   call:<func>         -> call <func> with <value> as arguments

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Source helpers (auto-detect location)
if [ -f "${SCRIPT_DIR}/helpers.bash" ]; then
  source "${SCRIPT_DIR}/helpers.bash" &>/dev/null
elif [ -f "/opt/utils/helpers.bash" ]; then
  source "/opt/utils/helpers.bash" &>/dev/null
else
  echo "ERROR: cannot find helpers.bash (tried ${SCRIPT_DIR}/helpers.bash and /opt/utils/helpers.bash)"
  exit 1
fi

# ---------------------------------------------------------------------------
# Internal: execute an action based on label pattern
# ---------------------------------------------------------------------------
_es_execute_action() {
  local label=$1 value=$2 mode=${3:-run}
  local action=${label%%:*}
  local target=${label#*:}
  # If no colon, target equals action (single-word labels like 'trace')
  [ "${target}" = "${label}" ] && target=""

  case "${action}" in
    cps)
      local rate=${value%%#*}
      local rampup_arg=""
      echo "${value}" | grep -q '#' && rampup_arg="--ramp-up-time ${value#*#}"
      if [ "${mode}" = "dry" ]; then
        echo "client_provision_cps ${target} ${rate} ${rampup_arg}"
      else
        client_provision_cps ${target} ${rate} ${rampup_arg}
      fi
      ;;
    trigger)
      if [ "${mode}" = "dry" ]; then
        echo "client_provision_trigger ${target} ${value}"
      else
        client_provision_trigger ${target} ${value} >/dev/null 2>&1
      fi
      ;;
    vault)
      if [ "${mode}" = "dry" ]; then
        echo "vault ${target}=${value}"
      else
        do_curl -XPOST -d'{"'${target}'":"'${value}'"}' \
          -H 'content-type:application/json' "$(admin_url)/vault" >/dev/null 2>&1
      fi
      ;;
    trace)
      if [ "${mode}" = "dry" ]; then
        echo "trace ${value}"
      else
        trace ${value} >/dev/null 2>&1
      fi
      ;;
    snapshot)
      if [ "${mode}" = "dry" ]; then
        echo "traffic_summary --save ${value}"
      else
        traffic_summary --save "${value}" >/dev/null 2>&1
      fi
      ;;
    *)
      if [ "${action}" = "call" ]; then
        if [ "${mode}" = "dry" ]; then
          echo "${target} ${value}"
        else
          if type "${target}" &>/dev/null; then
            ${target} ${value}
          else
            echo "  WARNING: '${target}' not found in shell"
          fi
        fi
      else
        echo "  WARNING: unknown action '${action}' for label '${label}'"
      fi
      ;;
  esac
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
usage() {
  cat << 'EOF'
Usage: events-schedule.sh <driver> [--from <seconds>] [--dry-run]

       Execute a timeline-driven event schedule using h2agent helpers (auto-loaded).
       Extensible via 'call:<func>' labels for any available shell function.

       driver:   TSV file with a Timeline header and labeled columns.
                 Labels encode actions directly.

                 Supported label patterns:

                 cps:<id>        Update CPS for a client provision id.
                                 Values: <rate> or <rate>#<rampup_seconds>
                 trigger:<id>    Trigger a client provision (value ignored).
                 vault:<key>     Set vault entry <key> to <value>.
                                 E.g. vault:RESPONSE_DELAY_MS can drive response
                                 delays when the server provision reads that variable.
                 trace           Set logging level to <value>.
                 snapshot        Save a traffic_summary snapshot named <value>.
                 call:<func>     Call <func> with <value> as arguments.
                                 Tip: for complex or multi-purpose functions, write thin
                                 wrappers that hardcode the fixed parameters and expose
                                 only the variable part as <value>. This also solves the
                                 case of needing multiple columns that call the same
                                 underlying function: create one wrapper per variant
                                 (e.g. call:deploy_fe and call:deploy_be both calling
                                 deploy internally with different defaults).
                                 Note: the scheduler is single-threaded. Blocking calls
                                 (sleeps, long I/O) delay subsequent events. Use background
                                 execution (&) in wrappers if the function may block.
                                 Timing uses wall clock: delayed events are not lost but
                                 execute back-to-back upon return (compressed, not skipped).

                 Use '-' as value to skip an action for a given timeline point.

       --from:   Timeline value (seconds) to start from. Events before this
                 point are skipped. Timing is adjusted so the first effective
                 event executes immediately (or after its delta from that point).

       --dry-run: Show resolved actions without executing them.

       Example driver file:

         Timeline(s)   cps:my_session    vault:RESPONSE_DELAY_MS   snapshot
         0             100               0                         before
         300           500#30            0                         -
         600           1000              200                       -
         900           0                 0                         after

       Example invocations:

         events-schedule.sh traffic.driver
         events-schedule.sh traffic.driver --from 300
         events-schedule.sh traffic.driver --dry-run
EOF
}

driver_file=""
start_at=0
dry_run=false

# Parse arguments
while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --from)
      start_at=$2
      shift 2
      ;;
    --dry-run)
      dry_run=true
      shift
      ;;
    *)
      driver_file=$1
      shift
      ;;
  esac
done

# Validations
[ -z "${driver_file}" ] && usage && exit 1
[ ! -f "${driver_file}" ] && echo "ERROR: cannot find driver '${driver_file}'" && exit 1

start_time=$(date +%s.%3N)
start_time_sec=${start_time%%.*}

# Read headers and determine column positions
header=""
read -r header < <(grep ^Timeline "${driver_file}")
column_names=()
column_positions=()

# Find the character position of each column label in the header
remaining="$header"
pos=0
while [ -n "$remaining" ]; do
  # Skip leading whitespace
  stripped="${remaining#"${remaining%%[![:space:]]*}"}"
  pos=$(( pos + ${#remaining} - ${#stripped} ))
  remaining="$stripped"
  [ -z "$remaining" ] && break
  # Extract token (non-space chars)
  token="${remaining%%[[:space:]]*}"
  column_names+=("$token")
  column_positions+=($pos)
  # Advance past this token
  remaining="${remaining#"$token"}"
  pos=$(( pos + ${#token} ))
done

echo
if [ $(echo "${start_at} > 0" | bc 2>/dev/null) = "1" ]; then
  echo "Resuming from timeline ${start_at}s (skipping earlier events)"
fi
[ "${dry_run}" = true ] && echo "DRY RUN mode (no actions will be executed)"
echo "[<date>] (+ <timeline> secs) | <values>"
echo "------------------------------------------------------------"

# Helper: extract and trim value at column position from a line
_extract_col() {
  local line=$1 col_idx=$2
  local start=${column_positions[$col_idx]}
  local end=${#line}
  if [ $((col_idx + 1)) -lt ${#column_positions[@]} ]; then
    end=${column_positions[$((col_idx + 1))]}
  fi
  local val="${line:$start:$((end - start))}"
  # Trim leading and trailing whitespace
  val="${val#"${val%%[![:space:]]*}"}"
  val="${val%"${val##*[![:space:]]}"}"
  echo "$val"
}

# Process timeline
last_printed_time=0
while IFS= read -r line; do
  # Extract timeline (first column)
  timeline=$(_extract_col "$line" 0)
  # Skip non-numeric lines
  [[ "$timeline" =~ ^[0-9]*\.?[0-9]+$ ]] || continue

  # Skip events before --from point
  if [ $(echo "${timeline} < ${start_at}" | bc) -eq 1 ]; then
    continue
  fi

  # Effective wait time adjusted by --from offset
  effective_timeline=$(echo "${timeline} - ${start_at}" | bc)

  # Wait until execution time (skip waiting in dry-run mode)
  if [ "${dry_run}" != true ]; then
    while true; do
      current_time=$(date +%s.%3N)
      current_time_sec=${current_time%%.*}
      elapsed_time_sec=$((current_time_sec - start_time_sec))

      if [ ${elapsed_time_sec} -ge ${effective_timeline%.*} ]; then
        break
      fi

      if (( current_time_sec > last_printed_time )); then
        echo "[$(date '+%H:%M:%S')] (+ $((elapsed_time_sec + ${start_at%.*})) secs) | waiting ..."
        last_printed_time=${current_time_sec}
      fi

      sleep 0.1
    done
  fi

  # Print event line
  echo "[$(date '+%H:%M:%S')] (+ ${timeline} secs) |"

  # Execute actions for each column
  for ((i = 1; i < ${#column_names[@]}; i++)); do
    label="${column_names[i]}"
    value=$(_extract_col "$line" $i)

    # Skip no-value markers
    [ "${value}" = "-" ] && continue
    [ -z "${value}" ] && continue

    if [ "${dry_run}" = true ]; then
      echo "  DRY: $(_es_execute_action "${label}" "${value}" dry)"
    else
      _es_execute_action "${label}" "${value}"
    fi
  done

done < "${driver_file}"

echo
echo "Done !"
