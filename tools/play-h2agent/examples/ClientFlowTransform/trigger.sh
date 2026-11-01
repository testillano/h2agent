#!/bin/bash
# Trigger and verify (sourced by run.sh, so helpers are available)
echo "Triggering client provision 'myFlow' ..."
do_curl -XPOST $(admin_url)/client-provision/myFlow/trigger
sleep 1
echo
echo "Client events:"
do_curl $(admin_url)/client-data
echo
echo "Server events:"
do_curl $(admin_url)/server-data
echo
echo "Vault:"
do_curl $(admin_url)/vault
