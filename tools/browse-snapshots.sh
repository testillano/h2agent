#!/bin/bash
# browse-snapshots.sh - offline, chronological "aerial view" over MANY h2agent
# snapshots from one test run (not the live cluster; not a per-mock helper).
#
# It is a generic traffic analyzer based on mock inspection: given N snapshots
# (one per mock) laid out under an AGREED HIERARCHY, it merges every stored event
# into a single timeline ordered by timestamp, writes a self-contained drill
# script per event (dumps that event's full HTTP/2 message), and lets you
# navigate them.
#
# Agreed hierarchy (what the `snapshot` helper produces, one directory per mock -
# the mock's "snapshot repository", e.g. a k8s pod dir or a native test mock dir):
#   <run-dir>/<snapdir>/<snapshot-repo>/server/data.json  # what the mock RECEIVED
#   <run-dir>/<snapdir>/<snapshot-repo>/client/data.json  # what the mock SENT
# The MOCK/actor label is the <snapshot-repo> directory basename; the ORIGIN
# label is the request's service-id header. Any consumer that lays snapshots out
# this way can use this analyzer as-is.
#
# A consumer (e.g. a wrapper script) may wrap it and inject its specifics via the
# following hook env vars. ALL are optional; with none set, the analyzer still
# works on any snapshot tree that follows the agreed hierarchy.
#
#   BROWSE_SNAPDIR
#       Name of the snapshots subdirectory under <run-dir>. If unset, the analyzer
#       autodetects a child dir named 'h2agents-snapshots' or 'h2a-snapshots', and
#       finally falls back to <run-dir> itself (repos as its direct children).
#
#   BROWSE_ACTOR_SED
#       A sed -E script applied to the MOCK/actor label (which defaults to the
#       snapshot-repo directory basename, minus a trailing k8s hash and a '-mock'
#       suffix). Use it to rename an actor for readability.
#       e.g. 's/^my-client-mock$/client/'
#
#   BROWSE_ORIGIN_SED
#       A sed -E script applied to the ORIGIN label only (ORIGIN defaults to the
#       request's 'service-id' header, or '-' if absent). Use it to shorten a long
#       service id to a friendly name. e.g. 's/.*my-long-service-id.*/svcA/'
#
#   BROWSE_TS0_SKIP_ACTOR
#       Name of an actor whose early events are pre-flow noise (e.g. a config or
#       bootstrap mock configured before the traffic starts). It ONLY changes where
#       the T=0 anchor is placed: T=0 becomes the first event whose actor is NOT
#       this one, so the T(ms) column measures from the real flow start instead of
#       from the bootstrap. It does NOT hide anything: those earlier events are
#       still listed, shown with T(ms)='-' (they precede T=0). Unset => T=0 is the
#       very first event (bootstrap included).
#
#   BROWSE_EXTRA_EVENTS_FN
#       Name of a shell function (exported with 'export -f') called as
#         FN "<RAW_tsv_file>" "<run-dir>"
#       to APPEND extra timeline rows in the 10-column TSV format. Use it to add a
#       leg that no mock recorded, recovered from another source (e.g. a service's
#       own log). Append nothing if there is nothing to add.
#
#   BROWSE_NOTES_FN
#       Name of a function printing extra NOTE/legend lines at the top, before the
#       list (e.g. to explain what this particular setup shows).
#
#   BROWSE_DRILL_FN
#       Name of a function called per event as
#         FN "<drill_file>" "<rel_data_file>" "<seqkind>" "<seq>" "<uri>" \
#            "<method>" "<origin>" "<dir>"
#       to write a CUSTOM drill script for events that are not backed by an
#       h2agent JSON (e.g. a row sourced from a log). Write the drill and return 0
#       to take over; return non-zero to let the core emit its default jq drill.
#
# TSV columns (tab-separated): ts actor dir method uri status file seqkind seq origin
#
# Default: print the timeline then navigate (Enter/p=step, l=list, <N>=jump,
# q=quit). --list just prints and exits.
#
# Usage:  browse-snapshots.sh <artifacts-run-dir> [--list]

set -uo pipefail

RUN_DIR=""
NAVIGATE=true
while [ $# -gt 0 ]; do
  case "$1" in
    --list)  NAVIGATE=false ; shift ;;
    -h|--help) awk 'NR==1{next} /^#/{sub(/^# ?/,""); print; next} {exit}' "$0" ; exit 0 ;;
    *) RUN_DIR="$1" ; shift ;;
  esac
done
[ -z "${RUN_DIR}" ] && { echo "Usage: browse-snapshots.sh <artifacts-run-dir> [--list]" ; exit 1 ; }

# Resolve the snapshots dir: explicit BROWSE_SNAPDIR, else autodetect common names.
if [ -n "${BROWSE_SNAPDIR:-}" ]; then
  SNAP_DIR="${RUN_DIR%/}/${BROWSE_SNAPDIR}"
else
  SNAP_DIR=""
  for cand in h2agents-snapshots h2a-snapshots snapshots; do
    [ -d "${RUN_DIR%/}/${cand}" ] && { SNAP_DIR="${RUN_DIR%/}/${cand}" ; break ; }
  done
  # also accept RUN_DIR being the snapshots dir itself (repos as direct children)
  [ -z "${SNAP_DIR}" ] && [ -d "${RUN_DIR%/}" ] && SNAP_DIR="${RUN_DIR%/}"
fi
[ -d "${SNAP_DIR}" ] || { echo "ERROR: no snapshots dir under '${RUN_DIR}'" ; exit 1 ; }

actor_of() {
  local a; a="$(basename "$1" | sed -E 's/-[a-f0-9]{6,10}-[a-z0-9]{5}$//; s/-mock$//')"
  [ -n "${BROWSE_ACTOR_SED:-}" ] && a="$(printf '%s' "${a}" | sed -E "${BROWSE_ACTOR_SED}")"
  printf '%s' "${a}"
}

RAW="$(mktemp)" ; ORDERED="$(mktemp)" ; trap 'rm -f "${RAW}" "${ORDERED}"' EXIT
sent_total=0

for repodir in "${SNAP_DIR}"/*/; do
  [ -d "${repodir}" ] || continue
  actor="$(actor_of "${repodir}")"

  sfile="${repodir}server/data.json"
  if [ -s "${sfile}" ]; then
    jq -r --arg a "${actor}" --arg f "${sfile}" '
      def origin($h): ($h["service-id"] // "") as $s |
        if ($s == "") then "-" else $s end;
      (. // []) | .[]? as $e | ($e.events // [])[]? |
      [ (.receptionTimestampUs // 0), $a, "RECV",
        ($e.method // "?"), ($e.uri // "?"), (.responseStatusCode // "?"),
        $f, "recvseq", (.recvseq // 0), origin(.requestHeaders // {}) ] | @tsv' \
      "${sfile}" 2>/dev/null >> "${RAW}"
  fi

  cfile="${repodir}client/data.json"
  if [ -s "${cfile}" ]; then
    n=$(jq -r '[.[]?.events[]?] | length' "${cfile}" 2>/dev/null || echo 0)
    sent_total=$(( sent_total + ${n:-0} ))
    jq -r --arg a "${actor}" --arg f "${cfile}" '
      (. // []) | .[]? as $p | ($p.events // [])[]? |
      [ (.sendingTimestampUs // .receptionTimestampUs // 0), "-", "SENT",
        ($p.method // "?"), ($p.uri // "?"), (.responseStatusCode // "?"),
        $f, "sendseq", (.sendseq // 0), $a ] | @tsv' \
      "${cfile}" 2>/dev/null >> "${RAW}"
  fi
done

# Consumer hook: append extra timeline rows (e.g. events recovered from SUT logs).
if [ -n "${BROWSE_EXTRA_EVENTS_FN:-}" ] && declare -F "${BROWSE_EXTRA_EVENTS_FN}" >/dev/null 2>&1; then
  "${BROWSE_EXTRA_EVENTS_FN}" "${RAW}" "${RUN_DIR}"
fi

# Optional consumer origin normalization: apply a sed script to the ORIGIN column
# only, so a consumer can shorten raw service-id values to friendly labels (e.g.
# my-long-service-id -> svcA) without the core knowing them.
if [ -n "${BROWSE_ORIGIN_SED:-}" ] && [ -s "${RAW}" ]; then
  : > "${RAW}.o"
  while IFS=$'\t' read -r ts actor dir method uri status file seqkind seq origin; do
    origin="$(printf '%s' "${origin}" | sed -E "${BROWSE_ORIGIN_SED}")"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "${ts}" "${actor}" "${dir}" "${method}" "${uri}" "${status}" "${file}" "${seqkind}" "${seq}" "${origin}" >> "${RAW}.o"
  done < "${RAW}"
  mv "${RAW}.o" "${RAW}"
fi

if [ ! -s "${RAW}" ]; then
  echo "No client/server data in snapshots. Was the run done with debug (data storage) enabled?"
  exit 0
fi

sort -n "${RAW}" > "${ORDERED}"
TOTAL=$(wc -l < "${ORDERED}" | tr -d ' ')

# T(ms) anchor: first real flow event. Optionally skip a pre-flow noise actor.
if [ -n "${BROWSE_TS0_SKIP_ACTOR:-}" ]; then
  TS0=$(awk -F'\t' -v skip="${BROWSE_TS0_SKIP_ACTOR}" '$2 != skip {print $1; exit}' "${ORDERED}")
else
  TS0=$(head -1 "${ORDERED}" | cut -f1)
fi
[ -z "${TS0}" ] && TS0=$(head -1 "${ORDERED}" | cut -f1)
: "${TS0:=0}"

# One self-contained drill script per event, under <run-dir>/drill/.
DRILL_DIR="${RUN_DIR%/}/drill"
rm -rf "${DRILL_DIR}" ; mkdir -p "${DRILL_DIR}"
n=0
while IFS=$'\t' read -r ts actor dir method uri status file seqkind seq origin; do
  n=$((n + 1)) ; tag="$(printf '%03d' "${n}")"
  rel="$(realpath --relative-to="${DRILL_DIR}" "${file}" 2>/dev/null || echo "${file}")"
  dfile="${DRILL_DIR}/${tag}.bash"
  # Consumer hook: custom drill for a non-h2agent seqkind (e.g. a SUT-log line). If the
  # hook declines (non-zero) or is absent, fall through to the default jq drill.
  if [ -n "${BROWSE_DRILL_FN:-}" ] && declare -F "${BROWSE_DRILL_FN}" >/dev/null 2>&1 \
     && "${BROWSE_DRILL_FN}" "${dfile}" "${rel}" "${seqkind}" "${seq}" "${uri}" "${method}" "${origin}" "${dir}"; then
    :
  else
    {
      echo "#!/bin/bash"
      echo "# ${actor} ${dir} ${method} ${uri} -> ${status}  (${seqkind}=${seq})"
      echo "# Full HTTP/2 message (request+response). Select by the group's"
      echo "# uri+method AND the per-endpoint ${seqkind} (which is NOT globally"
      echo "# unique: a client numbers it per endpoint, so two endpoints can share"
      echo "# ${seqkind}=1 - filtering by uri+method first disambiguates)."
      echo "cd \"\$(dirname \"\$0\")\""
      printf 'jq --arg uri %q --arg method %q '"'"'[.[] | select(.uri==$uri and (.method==$method or $method=="?")) | .events[] | select(.%s==%s)] | .[0] | {requestHeaders, requestBody, responseStatusCode, responseHeaders, responseBody}'"'"' %q\n' \
        "${uri}" "${method}" "${seqkind}" "${seq}" "${rel}"
    } > "${dfile}"
  fi
  chmod +x "${dfile}"
done < "${ORDERED}"

render_list() {
  local here="${1:-0}" i=0
  printf '   %-5s %-8s %-8s %-16s %-4s %-4s %-6s %s\n' "SEQ" "T(ms)" "ORIGIN" "MOCK" "DIR" "SC" "METHOD" "URI"
  while IFS=$'\t' read -r ts actor dir method uri status file seqkind seq origin; do
    i=$((i + 1)) ; local tag ; tag="$(printf '%03d' "${i}")"
    local ptr="   " ; [ "${i}" -eq "${here}" ] && ptr="-->"
    local tms ; if [ "${ts:-0}" -lt "${TS0:-0}" ]; then tms="-"; else tms=$(( ( ${ts:-0} - ${TS0:-0} ) / 1000 )); fi
    printf '%s%-5s %-8s %-8s %-16s %-4s %-4s %-6s %s\n' "${ptr}" "${tag}" "${tms}" "${origin:--}" "${actor}" "${dir}" "${status}" "${method}" "${uri}"
  done < "${ORDERED}"
}

drill_note() {
  echo "----------------------------------------------------------------------------"
  echo " Inspect any event's full HTTP/2 message (request + response). The SEQ number"
  echo " in each line is a ready-made script generated under this run:"
  echo "     bash ${DRILL_DIR}/<NNN>.bash          # e.g. bash ${DRILL_DIR}/003.bash"
  echo "----------------------------------------------------------------------------"
}

echo "============================================================================"
echo " Flow timeline  (from ${RUN_DIR})   ${TOTAL} events"
echo "============================================================================"
# Consumer hook: extra notes on top.
if [ -n "${BROWSE_NOTES_FN:-}" ] && declare -F "${BROWSE_NOTES_FN}" >/dev/null 2>&1; then
  "${BROWSE_NOTES_FN}"
fi
drill_note
render_list 0
if [ "${NAVIGATE}" != "true" ]; then
  exit 0
fi

# -------------------------- NAVIGATION (interactive) ------------------------
line_at() {
  local want="${1}" i=0
  while IFS=$'\t' read -r ts actor dir method uri status file seqkind seq origin; do
    i=$((i + 1))
    if [ "${i}" -eq "${want}" ]; then
      local tms ; if [ "${ts:-0}" -lt "${TS0:-0}" ]; then tms="-"; else tms=$(( ( ${ts:-0} - ${TS0:-0} ) / 1000 )); fi
      printf '   %-5s %-8s %-8s %-16s %-4s %-4s %-6s %s' "$(printf '%03d' "${i}")" "${tms}" "${origin:--}" "${actor}" "${dir}" "${status}" "${method}" "${uri}"
      return
    fi
  done < "${ORDERED}"
}

show_event() {
  local tag ; tag="$(printf '%03d' "${pos}")"
  {
    echo
    echo "============================================================================"
    echo " $(line_at "${pos}")"
    echo "============================================================================"
    bash "${DRILL_DIR}/${tag}.bash" 2>/dev/null | sed 's/^/  /'
  } | { if [ -t 1 ] && command -v less >/dev/null 2>&1; then less -FRX; else cat; fi ; }
}
show_full_list() {
  echo
  echo "============================================================================"
  echo " Timeline  (from ${RUN_DIR})   ${TOTAL} events"
  echo "============================================================================"
  render_list "${pos}"
}

pos=0
while :; do
  echo "----------------------------------------------------------------------------"
  echo -n " [Enter]=next  p=prev  l=list  <N>=go to  q=quit > "
  read -r key </dev/tty || break
  case "${key}" in
    q|Q) break ;;
    l|L) show_full_list ;;
    p|P) if [ "${pos}" -gt 1 ]; then pos=$((pos - 1)) ; fi ; show_event ;;
    ''|n|N) if [ "${pos}" -ge "${TOTAL}" ]; then echo ; echo "No more sequences !" ; break ; fi ; pos=$((pos + 1)) ; show_event ;;
    *[0-9]*) if [ "${key}" -ge 1 ] 2>/dev/null && [ "${key}" -le "${TOTAL}" ] 2>/dev/null; then pos="${key}" ; show_event ; fi ;;
  esac
done
echo
