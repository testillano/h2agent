#!/bin/bash
# Courtesy of Jesús Gallinal Moreno

SUBJ_common="/C=ES/ST=Madrid/L=Madrid/O=Security/OU=IT Department/CN="
SUBJ="${SUBJ_common}www.teslayout.com"
KEY_SIZE=2048
PASSWORD="hello"
DAYS=365
KEY_FILE="my_private_key.pem"
CRT_FILE="my_public_key_certificate.crt"

openssl req -newkey rsa:${KEY_SIZE} -keyout "${KEY_FILE}" -passout pass:"${PASSWORD}" -subj "${SUBJ}" -out "${CRT_FILE}" -x509 -days ${DAYS}

cat << EOF

  You could start the agent in this way:

  $> ./h2agent -l Debug --verbose --traffic-server-key ${KEY_FILE} --traffic-server-crt ${CRT_FILE} --secure-admin --traffic-server-key-password ${PASSWORD} &
  $> curl --insecure --http2-prior-knowledge https://localhost:8074/admin/v1/server-matching

  - or -

  Open firefox and accept the risk:

  $> firefox https://localhost:8074/admin/v1/server-matching

EOF

