#!/bin/bash
# Trigger 3 iterations at 1 cps and verify
echo "Triggering client provision 'myFlow' (3 iterations at 1 cps) ..."
do_curl "$(admin_url)/client-provision/myFlow?sequenceBegin=0&sequenceEnd=2&cps=1"
echo
echo "Waiting 4 seconds for completion ..."
sleep 4
echo
echo "Client events (expect 6: 3 GETs + 3 POSTs):"
do_curl $(admin_url)/client-data
echo
echo "Server events:"
do_curl $(admin_url)/server-data
