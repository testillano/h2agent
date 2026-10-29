#!/bin/bash
# Source code formatting with Artistic Style (astyle 3.1, default style), the
# same tool and version used by CI (Ubuntu runners ship astyle 3.1).
#
# Usage: ./format.sh [--check]
#
#   (no option)  Format sources in place.
#   --check      Dry run: list the files that would be formatted and exit 1 if
#                any (CI mode, nothing is modified).
#
# Native astyle 3.1 is used when available; otherwise it runs inside the
# project base image (ubuntu:24.04, astyle installed on the fly) as the
# current user, so formatted files are never left owned by root.

ASTYLE_VERSION="3.1"
SCR_DIR=$(readlink -f "$(dirname "$0")")
cd "${SCR_DIR}" || exit 1

check=
case "$1" in
  "") ;;
  --check) check=yes ;;
  -h|--help) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
  *) echo "Unknown option '$1' (try --help)"; exit 1 ;;
esac

# Same file set as the former build.sh formatting stage (plus native benchmarks):
mapfile -t sources < <(find src ut tools benchmark -name "*.hpp" -o -name "*.cpp" 2>/dev/null | grep -v '/CMakeFiles/' | sort)
[ ${#sources[@]} -eq 0 ] && echo "No sources found" && exit 1

opts=(--options=none --suffix=none --formatted)  # default style; no backups; report only changed files
[ -n "${check}" ] && opts+=(--dry-run)

if command -v astyle &>/dev/null && astyle --version | grep -q " ${ASTYLE_VERSION}\$"; then
  output=$(astyle "${opts[@]}" "${sources[@]}")
else
  echo "Native astyle ${ASTYLE_VERSION} not found: using docker (ubuntu:24.04) ..."
  output=$(docker run -i --rm -v "${PWD}:/data" -w /data ubuntu:24.04 bash -c "
    apt-get update -qq >/dev/null && apt-get install -y -qq astyle >/dev/null || exit 1
    setpriv --reuid=$(id -u) --regid=$(id -g) --clear-groups astyle ${opts[*]} \"\$@\"" _ "${sources[@]}") || {
    echo "ERROR: docker formatting failed"; exit 1; }
fi

formatted=$(echo "${output}" | sed -n 's/^Formatted  //p')
if [ -z "${formatted}" ]; then
  echo "Source code format OK (${#sources[@]} files checked)"
  exit 0
fi

if [ -n "${check}" ]; then
  echo "These files are not formatted (run ./format.sh to fix them):"
  echo "${formatted}" | sed 's/^/  /'
  exit 1
fi

echo "Formatted files:"
echo "${formatted}" | sed 's/^/  /'
exit 0
