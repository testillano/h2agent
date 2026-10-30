#!/bin/bash
# Trigger and verify (sourced by run.sh, so helpers are available).
#
# This example shows that a client provision chain ending in outState 'purge'
# removes ALL the client events it accumulated once the chain completes.
#
# Two conditions are required for the purge to actually happen, and this script
# makes them explicit so the result is not left to chance:
#
#   1) Client-data purge must be ENABLED. It is enabled by default, but it can be
#      turned off either at process start (--disable-purge) or at runtime. To make
#      this example self-contained we force it ON here via the admin REST API,
#      regardless of how the agent was started. If instead purge were DISABLED, the
#      chain would still run but the two client events would REMAIN (that is the
#      whole point of --disable-purge: keep events for forensics).
#
#   2) The chain must actually REACH the 'purge' step. Purge only fires at the
#      final step; if a step fails its expectedResponseStatusCode the chain breaks
#      earlier and the events are kept on purpose. Also, the chain runs
#      asynchronously (trigger returns immediately), so we must WAIT for it to
#      finish before checking -- a premature read would still show events.

echo "Ensuring client-data purge is ENABLED (independent of how the agent was started) ..."
echo "  PUT $(admin_url)/client-data/configuration?disablePurge=false"
curl -s --http2-prior-knowledge -XPUT "$(admin_url)/client-data/configuration?disablePurge=false" >/dev/null
echo

echo "Triggering client provision 'purgeFlow' (POST /api/v1/order -> GET /api/v1/order/A-100 -> purge) ..."
do_curl -XPOST $(admin_url)/client-provision/purgeFlow/trigger

# Wait for the asynchronous chain to complete and purge to run. Poll client-data
# until it is empty (HTTP 204 -> body "[]"/empty) instead of a blind sleep.
echo
echo "Waiting for the chain to complete and purge ..."
empty=false
for i in $(seq 1 20); do
  sleep 0.25
  cd_body=$(curl -s --http2-prior-knowledge $(admin_url)/client-data 2>/dev/null)
  if [ -z "${cd_body}" ] || [ "${cd_body}" = "[]" ]; then
    empty=true
    break
  fi
done

echo
echo "Client events after the chain reached 'purge':"
do_curl $(admin_url)/client-data
if ${empty}; then
  echo "  -> EMPTY, as expected: the whole client chain (both the POST and the GET events) was purged."
else
  echo "  -> NOT empty. Likely causes: purge is disabled, or a step failed its"
  echo "     expectedResponseStatusCode so the chain never reached 'purge'. Check"
  echo "     'client_data_configuration' (purgeExecution) and the client events above."
fi

echo
echo "Server events (the two requests the loopback server received are KEPT -- server purge is independent):"
do_curl $(admin_url)/server-data
