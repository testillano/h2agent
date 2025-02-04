#!/bin/bash

##########################
# CA-SIGNED certificates #
##########################
SCR_DIR=$(dirname $(readlink -f $0))

echo -e "\nRemember usage: $0 [server, i.e. 'www.teslayout.com' (defaults to 'localhost')]\n"
SERVER=${1:-localhost}

SUBJ_common="/C=ES/ST=Madrid/L=Madrid/O=Security/OU=IT Department/CN="
SUBJ_CA="${SUBJ_common}www.example.com"
SUBJ="${SUBJ_common}${SERVER}"

KEY_SIZE=2048
DAYS=365

CLIENT_KEY_FILE="ca-signed-client.key" # private pem
CLIENT_CRT_FILE="ca-signed-client.crt" # public pem
CLIENT_CSR_FILE="client.csr"
SERVER_KEY_FILE="ca-signed-server.key" # private pem
SERVER_CRT_FILE="ca-signed-server.crt" # public pem
SERVER_CSR_FILE="server.csr"
CA_KEY_FILE="ca.key" # private pem
CA_CRT_FILE="ca.crt" # public pem

# Create the CA Key and Certificate for signing Client Certs
openssl genrsa -des3 -out ${CA_KEY_FILE} ${KEY_SIZE}
openssl req -new -x509 -days ${DAYS} -key ${CA_KEY_FILE} -out ${CA_CRT_FILE} -subj "${SUBJ_CA}"

# Create the Server Key, CSR (Certificate Signing Request to allow CA issue certificates), and Certificate
openssl genrsa -des3 -out ${SERVER_KEY_FILE} ${KEY_SIZE}
openssl req -new -key ${SERVER_KEY_FILE} -out ${SERVER_CSR_FILE} -subj "${SUBJ}"

# We're self signing our own server cert here.  This is a no-no in production.
openssl x509 -req -days ${DAYS} -in ${SERVER_CSR_FILE} -CA ${CA_CRT_FILE} -CAkey ${CA_KEY_FILE} -set_serial 01 -out ${SERVER_CRT_FILE}

# Create the Client Key and CSR
openssl genrsa -des3 -out ${CLIENT_KEY_FILE} ${KEY_SIZE}
openssl req -new -key ${CLIENT_KEY_FILE} -out ${CLIENT_CSR_FILE} -subj "${SUBJ}"

# Sign the client certificate with our CA cert.  Unlike signing our own server cert, this is what we want to do.
# Serial should be different from the server one, otherwise curl will return NSS error -8054
openssl x509 -req -days ${DAYS} -in ${CLIENT_CSR_FILE} -CA ${CA_CRT_FILE} -CAkey ${CA_KEY_FILE} -set_serial 02 -out ${CLIENT_CRT_FILE}

# Verify Server Certificate
openssl verify -purpose sslserver -CAfile ${CA_CRT_FILE} ${SERVER_CRT_FILE}

# Verify Client Certificate
openssl verify -purpose sslclient -CAfile ${CA_CRT_FILE} ${CLIENT_CRT_FILE}

#all this is taken from https://github.com/nategood/sleep-tight/blob/master/scripts/create-certs.sh
#also see: https://gist.github.com/komuW/5b37ad11a320202c3408

# Hints (usage: $0 <url> <server key> <server crt> <server password> <client key> <client crt> <client password> <cacert>):
${SCR_DIR}/hints.sh https://localhost:8074/admin/v1/server-matching ${SERVER_KEY_FILE} ${SERVER_CRT_FILE} "<Server PEM Phrase>" ${CLIENT_KEY_FILE} ${CLIENT_CRT_FILE} "<Client PEM Phrase>" ${CA_CRT_FILE}

