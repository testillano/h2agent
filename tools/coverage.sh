#!/bin/bash
# Coverage report generation for UT, CT, and combined
#
# Usage: ./coverage.sh [ut|ct|all]
#        Default: all (generates ut/, ct/, combined/)

registry=ghcr.io/testillano
git_root_dir="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -z "$git_root_dir" ] && { echo "Go into the git repository !" ; exit 1 ; }

cd "${git_root_dir}"
echo
rm -rf coverage 2>/dev/null || { echo "You must remove './coverage' directory. I will do if you run: sudo chown -R $(id -u -n):$(id -g -n) coverage" ; exit 1 ; }

# Parse arguments
MODE=${1:-all}
case ${MODE} in
  ut|ct|all) ;;
  *) echo "Usage: $0 [ut|ct|all]" && exit 1 ;;
esac

# Base OS
echo "Base image (alpine/ubuntu) [ubuntu]:"
read -r os_type
[ -z "${os_type}" ] && os_type=ubuntu

bargs="--build-arg os_type=${os_type}"
bargs+=" --build-arg base_tag=latest"
bargs+=" --build-arg make_procs=$(grep processor /proc/cpuinfo -c)"

mkdir -p coverage/ut coverage/ct coverage/combined

###############
# UT COVERAGE #
###############
if [ "${MODE}" = "ut" ] || [ "${MODE}" = "all" ]; then
  echo
  echo "=== Building UT coverage image ==="
  docker build --rm ${bargs} -f Dockerfile.coverage.ut -t ${registry}/h2agent:latest-cov-ut . || exit 1

  echo
  echo "=== Running UT coverage ==="
  docker run -it --rm -v "${PWD}/coverage/ut:/code/coverage" ${registry}/h2agent:latest-cov-ut || exit 1

  echo "UT coverage generated in coverage/ut/"
fi

###############
# CT COVERAGE #
###############
if [ "${MODE}" = "ct" ] || [ "${MODE}" = "all" ]; then
  echo
  echo "=== Building CT coverage image ==="
  docker build --rm ${bargs} -f Dockerfile.coverage.ct -t ${registry}/h2agent:latest-cov-ct . || exit 1

  echo
  echo "=== Running CT coverage ==="
  TAG=latest-cov-ct XTRA_HELM_SETS="--set h2agent.coverage.enabled=true" ct/test.sh

  echo "Stopping h2agent gracefully (SIGTERM) to flush coverage data and process coverage ..."
  pod=$(kubectl --namespace ns-ct-h2agent get pod -o name -l app.kubernetes.io/name=h2agent | xargs basename)
  kubectl exec --namespace ns-ct-h2agent -it "${pod}" -- sh -c "kill \$(pgrep h2agent)"

  echo -n "Waiting for end of analysis ..."
  while true
  do
    sleep 1
    echo -n .
    kubectl exec --namespace ns-ct-h2agent ${pod} -- ls /tmp/exit_flag 2>/dev/null
    [ $? -ne 2 ] && break
  done

  echo "Copying report and restarting pod ..."
  kubectl --namespace ns-ct-h2agent cp ${pod}:/code/coverage ./coverage/ct &>/dev/null
  kubectl --namespace ns-ct-h2agent exec ${pod} -- rm /tmp/exit_flag

  echo
  echo "CT coverage generated in coverage/ct/"
fi

###########
# COMBINE #
###########
if [ "${MODE}" = "all" ]; then
  echo
  echo "=== Combining coverage reports ==="

  if [ -f coverage/ut/lcov.info ] && [ -f coverage/ct/lcov.info ]; then
    docker run --rm -v "${PWD}/coverage:/coverage" --entrypoint sh ${registry}/h2agent:latest-cov-ut -c \
      "lcov -a /coverage/ut/lcov.info -a /coverage/ct/lcov.info -o /coverage/combined/lcov.info --ignore-errors empty --ignore-errors negative && \
       genhtml /coverage/combined/lcov.info --output-directory /coverage/combined --ignore-errors negative"

    echo
    echo "Combined coverage generated in coverage/combined/"
  else
    echo "Both UT and CT are needed to combine"
  fi
fi

###############
# OPEN REPORT #
###############
report_dir="coverage/combined"
[ "${MODE}" = "ut" ] && report_dir="coverage/ut"
[ "${MODE}" = "ct" ] && report_dir="coverage/ct"

if [ -f "${report_dir}/index.html" ]; then
  if type firefox &>/dev/null; then
    echo "Opening ${report_dir}/index.html ..."
    firefox "${report_dir}/index.html" &
  elif type lynx &>/dev/null; then
    echo "Opening ${report_dir}/index.html ..."
    lynx "${report_dir}/index.html"
  else
    echo "Created ${report_dir}/index.html"
    echo "Install a browser to view: 'sudo apt install firefox' or 'sudo apt install lynx'"
    echo "Or execute: 'grep -oP 'headerCovTableEntry[^"]*">[0-9.]+' coverage/*/index.html'"
  fi
fi
