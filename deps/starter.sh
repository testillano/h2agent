#!/bin/sh

# $1: value to check
is_number() {
  echo "$1" | grep -qE '^[0-9]+$'
  [ $? -eq 0 ] && return 0
  echo "Invalid port number !"
  return 1
}

# $1: configuration file
launch_proxy() {
  sleep 0.5
  [ -f ${1} ] && exec nghttpx --conf=${1} --host-rewrite --no-server-rewrite # --no-add-x-forwarded-proto --log-level=INFO --no-via
}

if [ -n "${H2AGENT_TRAFFIC_PROXY_PORT}" ]
then
  fe_port=${H2AGENT_TRAFFIC_PROXY_PORT}
  be_port=${H2AGENT_TRAFFIC_SERVER_PORT:-8000}; default= ; [ -z "${H2AGENT_TRAFFIC_SERVER_PORT}" ] && default=" (default)"
  echo
  echo "Activating nghttpx proxy for traffic interface (detected variable 'H2AGENT_TRAFFIC_PROXY_PORT'):"
  echo "  H2AGENT_TRAFFIC_PROXY_PORT=${fe_port}" ; is_number ${fe_port} || exit 1
  echo "  H2AGENT_TRAFFIC_SERVER_PORT=${be_port}${default}" ; is_number ${be_port} || exit 1
  cat << EOF > /tmp/nghttpx-traffic.conf
# Traffic interface:
frontend=0.0.0.0,${fe_port};no-tls
backend=0.0.0.0,${be_port};;proto=h2;no-tls
http2-proxy=no
EOF
fi

if [ -n "${H2AGENT_ADMIN_PROXY_PORT}" ]
then
  fe_port=${H2AGENT_ADMIN_PROXY_PORT}
  be_port=${H2AGENT_ADMIN_SERVER_PORT:-8074}; default= ; [ -z "${H2AGENT_ADMIN_SERVER_PORT}" ] && default=" (default)"
  echo
  echo "Activating nghttpx proxy for administrative interface (detected variable 'H2AGENT_ADMIN_PROXY_PORT'):"
  echo "  H2AGENT_ADMIN_PROXY_PORT=${fe_port}" ; is_number ${fe_port} || exit 1
  echo "  H2AGENT_ADMIN_SERVER_PORT=${be_port}${default}" ; is_number ${be_port} || exit 1
  cat << EOF > /tmp/nghttpx-admin.conf
# Admin interface:
frontend=0.0.0.0,${fe_port};no-tls
backend=0.0.0.0,${be_port};;proto=h2;no-tls
http2-proxy=no
EOF
fi

# Traffic proxy
launch_proxy /tmp/nghttpx-traffic.conf &

# Admin proxy
launch_proxy /tmp/nghttpx-admin.conf &

# H2Agent
exec /opt/h2agent $@
