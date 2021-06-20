#!/bin/bash

SUBJ_common="/C=ES/ST=Madrid/L=Madrid/O=Security/OU=IT Department/CN="
SUBJ_CA="${SUBJ_common}www.example.com"
SUBJ="${SUBJ_common}www.teslayout.com"

set -x
# Create the CA Key and Certificate for signing Client Certs
openssl genrsa -des3 -out ca.key 4096
openssl req -new -x509 -days 365 -key ca.key -out ca.crt -subj "${SUBJ_CA}"

# Create the Server Key, CSR, and Certificate
openssl genrsa -des3 -out server.key 1024
openssl req -new -key server.key -out server.csr -subj "${SUBJ}"

# We're self signing our own server cert here.  This is a no-no in production.
openssl x509 -req -days 365 -in server.csr -CA ca.crt -CAkey ca.key -set_serial 01 -out server.crt

# Create the Client Key and CSR
openssl genrsa -des3 -out client.key 1024
openssl req -new -key client.key -out client.csr -subj "${SUBJ}"

# Sign the client certificate with our CA cert.  Unlike signing our own server cert, this is what we want to do.
# Serial should be different from the server one, otherwise curl will return NSS error -8054
openssl x509 -req -days 365 -in client.csr -CA ca.crt -CAkey ca.key -set_serial 02 -out client.crt

# Verify Server Certificate
openssl verify -purpose sslserver -CAfile ca.crt server.crt

# Verify Client Certificate
openssl verify -purpose sslclient -CAfile ca.crt client.crt

#all this is taken from https://github.com/nategood/sleep-tight/blob/master/scripts/create-certs.sh
#also see: https://gist.github.com/komuW/5b37ad11a320202c3408

# CURL examples:
#
# Start h2agent with admin secure:
#   ./h2agent --secure-admin -k client.key -c client.crt --server-key-password <password>
#
# Check server-matching insecurely:
#   curl --insecure --http2-prior-knowledge https://localhost:8074/provision/v1/server-matching

