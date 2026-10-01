#!/bin/bash
# tools/events-schedule.sh -- h2agent native event scheduler.
#
# Drives a timeline-based driver file (positional columns) that fires scheduled
# actions against h2agent helpers. Run with -h/--help for the full driver format,
# cell/argument semantics, label patterns and examples (the --help text is the
# single source of truth; this header intentionally stays minimal to avoid drift).

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Helper functions this scheduler needs. They are normally provided by the caller
# (e.g. sourcing tools/helpers.bash, or a '<role>_env' wrapper). We DECLARE the
# dependency and assert it -- we do NOT force a particular way of loading it.
# Only if the functions are missing do we try to source helpers.bash from the
# known locations (standalone use, e.g. inside the container). This lets a caller
# that already sourced the helpers use the scheduler with no extra setup.
_ES_REQUIRED_FNS="client_provision_cps client_provision_trigger admin_url do_curl metrics_summary trace"
_es_have_fns() { local f; for f in $_ES_REQUIRED_FNS; do command -v "$f" >/dev/null 2>&1 || return 1; done; return 0; }

if ! _es_have_fns; then
  # not already available -> try the known helper locations
  if [ -f "${SCRIPT_DIR}/helpers.bash" ]; then
    source "${SCRIPT_DIR}/helpers.bash" &>/dev/null
  elif [ -f "/opt/utils/helpers.bash" ]; then
    source "/opt/utils/helpers.bash" &>/dev/null
  fi
fi

# assert: whether provided by the caller or just sourced, the functions must exist
if ! _es_have_fns; then
  echo "ERROR: required h2agent helper functions not available." >&2
  echo "       Source the helpers first (e.g. tools/helpers.bash, or a '<role>_env' wrapper)." >&2
  echo "       Missing at least one of: ${_ES_REQUIRED_FNS}" >&2
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
      # SHORTCUT for client_provision_cps. Positional cell: <rate> [rampup_seconds].
      #   "100"     -> client_provision_cps <id> 100
      #   "100 60"  -> client_provision_cps <id> 100 --ramp-up-time 60
      # For the full power of client_provision_cps (--repeat, --in-state, ...) use
      # 'call:client_provision_cps' with the native arguments instead.
      eval "set -- ${value}"
      local _rate=$1 _rampup=$2 _cps_args=()
      [ -n "${_rate}" ] && _cps_args+=("${_rate}")
      [ -n "${_rampup}" ] && _cps_args+=(--ramp-up-time "${_rampup}")
      if [ "${mode}" = "dry" ]; then
        echo "client_provision_cps ${target} ${_cps_args[*]}"
      else
        client_provision_cps "${target}" "${_cps_args[@]}"
      fi
      ;;
    trigger)
      eval "set -- ${value}"
      if [ "${mode}" = "dry" ]; then
        echo "client_provision_trigger ${target} $*"
      else
        client_provision_trigger "${target}" "$@" >/dev/null 2>&1
      fi
      ;;
    vault)
      # atomic value (a single vault payload string)
      if [ "${mode}" = "dry" ]; then
        echo "vault ${target}=${value}"
      else
        do_curl -XPOST -d'{"'"${target}"'":"'"${value}"'"}' \
          -H 'content-type:application/json' "$(admin_url)/vault" >/dev/null 2>&1
      fi
      ;;
    trace)
      # atomic value (a single log level)
      if [ "${mode}" = "dry" ]; then
        echo "trace ${value}"
      else
        trace "${value}" >/dev/null 2>&1
      fi
      ;;
    metrics-snapshot)
      # SHORTCUT for 'metrics_summary --save <label>'. The cell is the snapshot label.
      # For the full power of metrics_summary (--now, --delta, --json, <ref1> <ref2>, ...)
      # use 'call:metrics_summary' with native arguments.
      if [ "${mode}" = "dry" ]; then
        echo "metrics_summary --save ${value}"
      else
        metrics_summary --save "${value}" >/dev/null 2>&1
      fi
      ;;
    *)
      if [ "${action}" = "call" ]; then
        # call:<func> -- arbitrary args to <func>, shell-tokenized from the cell.
        eval "set -- ${value}"
        if [ "${mode}" = "dry" ]; then
          echo "${target} $*"
        else
          if type "${target}" &>/dev/null; then
            "${target}" "$@"
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

       driver:   Text file with a 'Timeline(s)' header row and labeled columns.

                 TIMELINE
                 The first column is 'Timeline(s)': wall-clock seconds from the start
                 of the run at which that row's actions fire. Rows are processed in order;
                 the scheduler waits until each row's time before executing it.

                 COLUMNS / LABELS (positional)
                 Columns are positional, aligned to the header. Each header LABEL marks
                 the START column of its field; a data CELL spans from its own column's
                 start to the next column's start (end-of-line for the last column).
                 Labels encode the action to run (see 'label patterns' below) and must
                 NOT contain spaces -- a space in the header starts a new column.

                 CELL VALUE (how a cell becomes the action's arguments)
                 A cell is first TRIMMED of surrounding spaces (so its content may sit
                 anywhere within the column width -- left, centered or right -- it does
                 not matter). The trimmed cell is then tokenized with shell word-splitting
                 (eval "set -- <cell>") and passed to the action, so it behaves exactly
                 like typing those arguments on a shell command line:
                   100                   -> 1 arg : 100
                   -l Debug --verbose    -> 3 args: -l, Debug, --verbose (MULTI-ARG)
                   "a b c"               -> 1 arg : "a b c"   (quoted = atomic, spaces kept)
                   "  a b"               -> 1 arg : "  a b"   (leading spaces kept)
                   "a b" "x y z"         -> 2 args: "a b", "x y z"
                 Quote a token to keep it atomic; leave tokens unquoted to split them.
                 SKIP: an EMPTY cell, or a single '-' (anywhere in the column -- it is
                 trimmed), performs NO action for that timeline point. Because '-' is the
                 skip marker it cannot be passed as a literal argument (wrap the target if
                 you ever need a literal '-').
                 SECURITY: cells are eval'd, so the driver is TRUSTED input -- shell
                 expansions in a cell (e.g. $(cmd), `cmd`, $VAR, ;, &&) ARE executed. Do
                 not run drivers from untrusted sources.

                 COLUMNS ARE CALLERS
                 Every column calls a function with its cell as the arguments. 'call:'
                 is the generic form; the rest are shortcuts over a specific helper (use
                 'call:<helper>' instead when you need the helper's full argument set).

                 Generic caller:
                 call:<func>     Call <func> with the cell as arguments.
                                 Tip: for complex or multi-purpose functions, write thin
                                 wrappers that hardcode the fixed parameters and expose
                                 only the variable part in the cell. This also solves the
                                 case of needing multiple columns that call the same
                                 underlying function: create one wrapper per variant
                                 (e.g. call:deploy_fe and call:deploy_be both calling
                                 deploy internally with different defaults).
                                 Note: the scheduler is single-threaded. Blocking calls
                                 (sleeps, long I/O) delay subsequent events. Use background
                                 execution (&) in wrappers if the function may block.
                                 Timing uses wall clock: delayed events are not lost but
                                 execute back-to-back upon return (compressed, not skipped).

                 PREAMBLE (self-contained wrappers)
                 Lines starting with '#@' are sourced (one bash statement each) BEFORE the
                 timeline runs, so a driver can define its own wrapper functions and stay
                 self-contained -- no external files needed. Typically you define thin
                 wrappers here and invoke them from 'call:<wrapper>' columns. Shell
                 redirections/pipes belong INSIDE the wrapper (a cell cannot carry a '>'
                 or '|', since cells become arguments, not a command line). ROOT_DIR (and
                 any env the caller exported) is available to the wrappers.
                   #@ gong() { printf '\\a>>> %s <<<\\n' "$1"; }                    # audible/visual marker
                   #@ freeze() { kubectl -n "$1" scale deploy/"$2" --replicas=0; }  # kill a dep mid-run

                 ...then in the table:
                   Timeline(s)  cps:myFlow  call:gong      call:freeze
                   0            100         run-started    -
                   120          100         halfway        myNS myDeployment
                   300          0           end            -

                 Note: '#@' lines are also eval'd/sourced, so (like cells) the driver is
                 TRUSTED input. The preamble is skipped in --dry-run.

                 Shortcut callers:
                 cps:<id>        -> client_provision_cps. Positional cell:
                                 "<rate> [rampup_seconds]", e.g. "100" or "100 60".
                                 Full power: call:client_provision_cps (--repeat,
                                 --in-state, ...).
                 trigger:<id>    -> client_provision_trigger (cell may add args).
                 vault:<key>     -> set vault entry <key> to the cell (atomic string).
                                 E.g. vault:RESPONSE_DELAY_MS can drive response
                                 delays when the server provision reads that variable.
                 trace           -> set logging level to the cell (atomic).
                 metrics-snapshot    -> metrics_summary --save <label> (cell = label).
                                 Full power: call:metrics_summary (--now, --delta,
                                 --json, <ref1> <ref2>, ...).

       --from:   Timeline value (seconds) to start from. Events before this
                 point are skipped. Timing is adjusted so the first effective
                 event executes immediately (or after its delta from that point).

       --dry-run: Show resolved actions without executing them.

       Example driver file:

         Timeline(s)   cps:my_session   vault:RESPONSE_DELAY_MS   metrics-snapshot
         0             100              0                         before
         300           500 60           0                         -
         600           1000             200                       -
         900           0                0                         after

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

# Preamble: source the driver's '#@' setup lines (one bash statement per line) BEFORE the
# timeline runs. Use them to define thin wrapper functions the driver's 'call:' columns
# invoke, keeping the driver self-contained. Each '#@' line is sourced as-is (shell code),
# so -- like the eval'd cells -- the driver is TRUSTED input. Not run in --dry-run.
if [ "${dry_run}" != true ]; then
  _es_preamble="$(sed -n 's/^#@[[:space:]]\?//p' "${driver_file}")"
  [ -n "${_es_preamble}" ] && source /dev/stdin <<< "${_es_preamble}"
fi

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
# Read the driver on a DEDICATED file descriptor, NOT stdin: actions executed per row
# (client_provision_cps/trigger/vault -> do_curl -> curl, and metrics_summary) read from
# stdin and would otherwise consume the rest of the driver, making the loop exit after the
# first row (premature EOF). We let bash auto-allocate a free fd (>=10) into ${drvfd}
# instead of hardcoding one, so there is no risk of clashing with an fd the caller already
# uses (requires bash >= 4.1; the project already relies on modern bash).
while IFS= read -r -u "${drvfd}" line; do
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

done {drvfd}< "${driver_file}"

echo
echo "Done !"
