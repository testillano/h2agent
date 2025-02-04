#!/bin/bash
#ENABLE_CHECKS=true

#############################################################
# SELF-SIGNED certificates to avoid Certification Authority #
# (Courtesy of Jesús Gallinal Moreno)                       #
#############################################################
SCR_DIR=$(dirname $(readlink -f $0))

echo -e "\nRemember usage: $0 [server, i.e. 'www.teslayout.com' (defaults to 'localhost')]\n"
SERVER=${1:-localhost}

SUBJ_common="/C=ES/ST=Madrid/L=Madrid/O=Security/OU=IT Department/CN="
SUBJ="${SUBJ_common}${SERVER}"

KEY_SIZE=2048
PASSWORD="1111" # PEM phrase (simplify script using same for both client and server)
DAYS=365

CLIENT_KEY_FILE="self-signed-client.key" # private pem
CLIENT_CRT_FILE="self-signed-client.crt" # public pem
SERVER_KEY_FILE="self-signed-server.key" # private pem
SERVER_CRT_FILE="self-signed-server.crt" # public pem

openssl req -newkey rsa:${KEY_SIZE} -keyout "${CLIENT_KEY_FILE}" -passout pass:"${PASSWORD}" -subj "${SUBJ}" -out "${CLIENT_CRT_FILE}" -x509 -days ${DAYS} # generate key & certificate for client
openssl req -newkey rsa:${KEY_SIZE} -keyout "${SERVER_KEY_FILE}" -passout pass:"${PASSWORD}" -subj "${SUBJ}" -out "${SERVER_CRT_FILE}" -x509 -days ${DAYS} # generate key & certificate for server

# Alternative learnt from ChatGPT:
#openssl rand -out ${HOME}/.rnd -hex 256 # create random seed (just in case it does not exists), in order to prompt for PEM phrase:
#openssl genpkey -algorithm RSA -out <key file> -pkeyopt rsa_keygen_bits:${KEY_SIZE} -aes256
#openssl req -x509 -key <key file> -out <crt file> -days ${DAYS} -subj "${SUBJ}" # generate self-signed certificate using the private key

# Checkings:
if [ -n "${ENABLE_CHECKS}" ]
then
  echo "Press ENTER to check client private key ..." ; read -r dummy ;       openssl rsa -in ${CLIENT_KEY_FILE} -check
  echo "Press ENTER to see client certificate content ..." ; read -r dummy ; openssl x509 -in ${CLIENT_CRT_FILE} -text -noout
  echo "Press ENTER to check server private key ..." ; read -r dummy ;       openssl rsa -in ${SERVER_KEY_FILE} -check
  echo "Press ENTER to see server certificate content ..." ; read -r dummy ; openssl x509 -in ${SERVER_CRT_FILE} -text -noout
fi

# Hints (usage: $0 <url> <server key> <server crt> <server password> <client key> <client crt> <client password> <cacert (*)>):
# (*): in order to make client trust the server self-signed certificate, use --cacert with it because there is no CA
${SCR_DIR}/hints.sh https://localhost:8074/admin/v1/server-matching ${SERVER_KEY_FILE} ${SERVER_CRT_FILE} ${PASSWORD} ${CLIENT_KEY_FILE} ${CLIENT_CRT_FILE} ${PASSWORD} ${SERVER_CRT_FILE}

