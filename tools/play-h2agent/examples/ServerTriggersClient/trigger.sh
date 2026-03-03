#!/bin/bash
# Trigger and verify (sourced by run.sh, so helpers are available)
echo "Sending webhook notification ..."
do_curl -d '{"event":"user.created","userId":42}' $(traffic_url)/api/v1/webhook/notify
sleep 1
echo
echo "Server events (should show both /webhook/notify and /forward):"
do_curl $(admin_url)/server-data
echo
echo "Client events (should show POST /api/v1/forward with notification body):"
do_curl $(admin_url)/client-data
