#!/bin/echo "source me!"

#############
# VARIABLES #
#############
PNAME=${PNAME:-h2agent}

TRAFFIC_PORT=${TRAFFIC_PORT:-8000} # maybe proxy on 8001 ...
[ "${TRAFFIC_SERVER_API}" = "/" ] && TRAFFIC_SERVER_API=
ADMIN_PORT=${ADMIN_PORT:-8074} # maybe proxy on 8075 ...
ADMIN_SERVER_API="admin/v1"
METRICS_PORT=${METRICS_PORT:-8080}

SCHEME=${SCHEME:-http}
CURL=${CURL:-"curl -s -i --http2-prior-knowledge"} # may be just --http2, --http1.0, --http1.1, or nothing
SERVER_ADDR=${SERVER_ADDR:-localhost}

BEAUTIFY_JSON=yes

# Per-session temp file (allows parallel helper invocations)
_H2A_CURL_OUT="/tmp/curl.out.$$"
trap 'rm -f ${_H2A_CURL_OUT} ${_H2A_CURL_OUT}.sorted' EXIT

#############
# FUNCTIONS #
#############

# -----------------------------------------------------------------------------
# INTERNAL

traffic_url() {
  echo "${SCHEME}://${SERVER_ADDR}:${TRAFFIC_PORT}"
}

admin_url() {
  echo -n "${SCHEME}://${SERVER_ADDR}:${ADMIN_PORT}/${ADMIN_SERVER_API}"
}

metrics_url() {
  echo "${SCHEME}://${SERVER_ADDR}:${METRICS_PORT}/metrics"
}

do_curl() {
  echo
  echo [${CURL} "$@"]
  echo
  ${CURL} "$@" | tee ${_H2A_CURL_OUT}
  [ $? -ne 0 ] && return 1

  [ -n "${PLAIN}" ] && echo && return 0 # special for trace()

  # Last empty line or no line feed (no body answered):
  [ -z $(tail -c 1 ${_H2A_CURL_OUT}) ] && return 0

  [ -n "${BEAUTIFY_JSON}" ] && echo -e "\n\nPRETTY BODY PRINTOUT (disable on curl operation unsetting 'BEAUTIFY_JSON'):" && pretty
  echo -e "\n\n(type 'pretty' or 'raw' to isolate body printout)\n"
}

# -----------------------------------------------------------------------------
# GENERAL RESOURCES

schema() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: schema [-h|--help] [--clean] [file]; Cleans/gets/updates current schema configuration ($(admin_url)/schema)." && return 0
  [ -z "$1" ] && do_curl $(admin_url)/schema && return 0
  if [ "$1" = "--clean" ]
  then
    do_curl -XDELETE $(admin_url)/schema
  else
    do_curl -XPOST -d@${1} -H 'content-type:application/json' $(admin_url)/schema
  fi
}

vault() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: vault [-h|--help] [--clean] [name|file]; Cleans/gets/updates current agent vault configuration ($(admin_url)/vault)." && return 0
  [ -z "$1" ] && do_curl $(admin_url)/vault && return 0
  local queryParam=
  if [ "$1" = "--clean" ]
  then
    [ -n "$2" ] && queryParam="?name=$2"
    do_curl -XDELETE $(admin_url)/vault${queryParam}
  else
    [ -n "$1" ] && queryParam="?name=$1"
    if [ -f "$1" -o -p "$1" -o -r "$1" ]
    then
      do_curl -XPOST -d@${1} -H 'content-type:application/json' $(admin_url)/vault
    else
      do_curl $(admin_url)/vault${queryParam}
    fi
  fi
}

files() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: files [-h|--help]; Gets the files processed."
    return 0
  else
    do_curl $(admin_url)/files
  fi
}

files_configuration() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: files_configuration [-h|--help]; Manages files configuration (gets current status by default)."
    echo "                            [--enable-read-cache]  ; Enables cache for read operations."
    echo "                            [--disable-read-cache] ; Disables cache for read operations."
    return 0
  elif [ "$1" = "--enable-read-cache" ]
  then
    do_curl -XPUT "$(admin_url)/files/configuration?readCache=true"
  elif [ "$1" = "--disable-read-cache" ]
  then
    do_curl -XPUT "$(admin_url)/files/configuration?readCache=false"
  else
    do_curl $(admin_url)/files/configuration
  fi
}

udp_sockets() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: udp_sockets [-h|--help]; Gets the udp sockets processed."
    return 0
  else
    do_curl $(admin_url)/udp-sockets
  fi
}

configuration() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: configuration [-h|--help]; Gets agent general static configuration."
    return 0
  else
    do_curl $(admin_url)/configuration
  fi
}

# -----------------------------------------------------------------------------
# TRAFFIC SERVER

server_configuration() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: server_configuration [-h|--help]; Manages agent server configuration (gets current status by default)."
    echo "                            [--traffic-server-ignore-request-body]             ; Ignores request body on server receptions."
    echo "                            [--traffic-server-receive-request-body]            ; Processes request body on server receptions."
    echo "                            [--traffic-server-dynamic-request-body-allocation] ; Does dynamic request body memory allocation on server receptions."
    echo "                            [--traffic-server-initial-request-body-allocation] ; Pre reserves request body memory on server receptions."
    return 0
  elif [ "$1" = "--traffic-server-ignore-request-body" ]
  then
    do_curl -XPUT "$(admin_url)/server/configuration?receiveRequestBody=false"
  elif [ "$1" = "--traffic-server-receive-request-body" ]
  then
    do_curl -XPUT "$(admin_url)/server/configuration?receiveRequestBody=true"
  elif [ "$1" = "--traffic-server-dynamic-request-body-allocation" ]
  then
    do_curl -XPUT "$(admin_url)/server/configuration?preReserveRequestBody=false"
  elif [ "$1" = "--traffic-server-initial-request-body-allocation" ]
  then
    do_curl -XPUT "$(admin_url)/server/configuration?preReserveRequestBody=true"
  else
    do_curl $(admin_url)/server/configuration
  fi
}

server_data_configuration() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: server_data_configuration [-h|--help]; Manages agent server data configuration (gets current status by default)."
    echo "                                 [--discard-all]     ; Discards all the events processed."
    echo "                                 [--discard-history] ; Keeps only the last event processed for a key."
    echo "                                 [--keep-all]        ; Keeps all the events processed."
    echo "                                 [--disable-purge]   ; Skips events post-removal when a provision on 'purge' state is reached."
    echo "                                 [--enable-purge]    ; Processes events post-removal when a provision on 'purge' state is reached."
    return 0
  elif [ "$1" = "--discard-all" ]
  then
    do_curl -XPUT "$(admin_url)/server-data/configuration?discard=true&discardKeyHistory=true"
  elif [ "$1" = "--discard-history" ]
  then
    do_curl -XPUT "$(admin_url)/server-data/configuration?discard=false&discardKeyHistory=true"
  elif [ "$1" = "--keep-all" ]
  then
    do_curl -XPUT "$(admin_url)/server-data/configuration?discard=false&discardKeyHistory=false"
  elif [ "$1" = "--disable-purge" ]
  then
    do_curl -XPUT "$(admin_url)/server-data/configuration?disablePurge=true"
  elif [ "$1" = "--enable-purge" ]
  then
    do_curl -XPUT "$(admin_url)/server-data/configuration?disablePurge=false"
  else
    do_curl $(admin_url)/server-data/configuration
  fi
}

server_matching() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: server_matching [-h|--help] [file]; Gets/updates current server matching configuration ($(admin_url)/server-matching)." && return 0
  [ -z "$1" ] && do_curl $(admin_url)/server-matching && return 0
  do_curl -XPOST -d@${1} -H 'content-type:application/json' $(admin_url)/server-matching
}

server_provision() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: server_provision [-h|--help] [--clean] [file]; Cleans/gets/updates current server provision configuration ($(admin_url)/server-provision)." && return 0
  [ -z "$1" ] && do_curl $(admin_url)/server-provision && return 0
  if [ "$1" = "--clean" ]
  then
    do_curl -XDELETE $(admin_url)/server-provision
  else
    do_curl -XPOST -d@${1} -H 'content-type:application/json' $(admin_url)/server-provision
  fi
}

server_provision_unused() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: server_provision_unused [-h|--help]; Get current server provision configuration still not used ($(admin_url)/server-provision/unused)." && return 0
  do_curl $(admin_url)/server-provision/unused && return 0
}

server_data() {
  local clean=
  local surf=
  local dump=

  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: server_data [-h|--help]; Inspects server data events ($(admin_url)/server-data)."
    echo "                   [method] [uri] [[-]event number] [event path] ; Positional filters to narrow the server data selection."
    echo "                                                                   Event number may be negative to access by reverse chronological order."
    echo "                   [--summary] [max keys]          ; Gets current server data summary to guide further queries."
    echo "                                                     Displayed keys (method/uri) could be limited (10 by default, -1: no limit)."
    echo "                   [--clean] [query filters]       ; Removes server data events."
    echo "                   [--surf] [query filters]        ; Interactive sorted (regardless method/uri) server data navigation."
    echo "                   [--dump] [query filters]        ; Dumps all sequences detected for server data under 'server-data-sequences' directory."
    return 0
  elif [ "$1" = "--summary" ]
  then
    local maxKeys=${2:-10}
    local queryParams=
    [ -n "${maxKeys}" ] && queryParams="?maxKeys=${maxKeys}"
    [ "${maxKeys}" = "-1" ] && queryParams=
    do_curl "$(admin_url)/server-data/summary${queryParams}"
    return 0
  elif [ "$1" = "--clean" ]
  then
    clean=yes
    shift
  elif [ "$1" = "--surf" ]
  then
    surf=yes
    shift
  elif [ "$1" = "--dump" ]
  then
    dump=yes
    shift
  fi

  local requestMethod=$1
  local requestUri=$2
  local eventNumber=$3
  local eventPath=$4

  local keyIndicators=""
  [ -n "${requestMethod}" ] && keyIndicators+="x"
  [ -n "${requestUri}" ] && keyIndicators+="x"
  [ "${keyIndicators}" = "x" ] && echo "Error: both method & uri must be provided" && return 1
  [ -n "${eventNumber}" -a -z "${keyIndicators}" ] && echo "Error: method/uri are also required when eventNumber is provided" && return 1
  [ -n "${eventPath}" -a -z "${eventNumber}" ] && echo "Error: eventNumber is required when eventPath is provided" && return 1

  local queryParams=
  [ -n "${requestMethod}" ] && queryParams="?requestMethod=${requestMethod}"

  local curl_method=
  [ -n "${clean}" ] && curl_method="-XDELETE"

  local devnull=
  [ -n "${dump}" -o -n "${surf}" ] && devnull=">/dev/null"

  if [ -n "${requestUri}" ]
  then
    local urlencode=
    [ -n "${requestUri}" ] && urlencode="--data-urlencode requestUri=\"${requestUri}\""
    [ -n "${eventNumber}" ] && urlencode+=" --data-urlencode eventNumber=${eventNumber}"
    [ -n "${eventPath}" ] && urlencode+=" --data-urlencode eventPath=${eventPath}"
    eval do_curl ${curl_method} -G ${urlencode} "$(admin_url)/server-data${queryParams}" ${devnull}
  else
    if [ -z "${clean}${dump}${surf}" ]
    then
      echo
      echo "Take care about storage size when querying without filters."
      echo "You may want to check the summary before: server_data --summary"
      echo
      echo "Press ENTER to continue, CTRL-C to abort ..."
      read -r dummy
    fi
    eval do_curl ${curl_method} "$(admin_url)/server-data" ${devnull}
  fi

  [ -z "${clean}${dump}${surf}" ] && return 0

  local sequences=
  if [ -n "${requestUri}" ]; then sequences=( $(pretty ".events[].recvseq" | sort -n) ) ; else sequences=( $(pretty ".[].events[].recvseq" | sort -n) ) ; fi
  local indx_max=${#sequences[@]}

  if [ -n "${dump}" ]
  then
    mkdir -p server-data-sequences
    local arrayPrefix=
    [ -z "${requestUri}" ] && arrayPrefix=".[] | "
    for s in ${sequences[@]}; do pretty | jq "${arrayPrefix}select (.events[].recvseq == $s) | del (.events[] | select (.recvseq != $s))" > server-data-sequences/$(printf "%02d\n" "$s").json ; done

  elif [ -n "${surf}" ]
  then
    if [ -n "${requestUri}" ]
    then
      pretty | jq '.method as $method | .uri as $uri | .provisionUri as $provisionUri | .events | map({events: [.], method: $method, uri: $uri, provisionUri: $provisionUri})' > ${_H2A_CURL_OUT}.sorted
    else
      pretty | jq 'map(. as $parent | .events |= sort_by(.recvseq) | .events[] | {events: [.], method: $parent.method, uri: $parent.uri, provisionUri: $parent.provisionUri}) | sort_by(.events[].recvseq)' > ${_H2A_CURL_OUT}.sorted
    fi

    local indx=0
    [ ! -s ${_H2A_CURL_OUT}.sorted ] && echo && cat ${_H2A_CURL_OUT} && return 0
    echo
    echo "Show also corresponding (p)rovisions or just show server [d]ata:"
    read -r opt
    [ -z "${opt}" ] && opt=d
    [ "${opt}" = "p" ] && server_provision >/dev/null
    while true
    do
      echo -e "\nMessage $((indx+1)):\n"
      jq ".[$indx]" ${_H2A_CURL_OUT}.sorted

      # Corresponding provision:
      if [ "${opt}" = "p" ]
      then
        local requestUri=$(jq -r ".[$indx].provisionUri" ${_H2A_CURL_OUT}.sorted | sed 's/\\/\\\\/g')
        if [ "${requestUri}" != "null" ]
        then
          echo -e "\nCorresponding provision processed:\n"
          local inState=$(jq -r ".[$indx].events[0].previousState" ${_H2A_CURL_OUT}.sorted)
          local requestMethod=$(jq -r ".[$indx].method" ${_H2A_CURL_OUT}.sorted)
          if [ "${inState}" == "initial" ]
          then
            local cond="(.inState == \"initial\" or .inState == null) and .requestMethod == \""${requestMethod}"\" and .requestUri == \""${requestUri}"\""
            pretty | jq ".[] | select (${cond})" | sed 's/^/      /'
          else
            local cond=".inState == \""${inState}"\" and .requestMethod == \""${requestMethod}"\" and .requestUri == \""${requestUri}"\""
            pretty | jq ".[] | select (${cond})" | sed 's/^/      /'
          fi
        fi
      fi

      indx=$((indx+1))
      [ $indx -eq $indx_max ] && echo -e "\n\nThat's all ! ($indx_max events)\n" && return 0
      echo -e "\n\nPress ENTER to print next sequence ..."
      read -r dummy
    done
  fi
}

check_storage() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: check_storage [-h|--help]; Checks if loaded provisions need event storage but it is disabled."
    echo "                     [--server] ; Check server provisions only."
    echo "                     [--client] ; Check client provisions only."
    echo "                     By default, checks both server and client."
    echo "                     Returns 0 if consistent, 1 if storage is needed but disabled."
    return 0
  fi

  local check_server=true
  local check_client=true
  [ "$1" = "--server" ] && check_client=false
  [ "$1" = "--client" ] && check_server=false

  local rc=0

  if [ "${check_server}" = "true" ]
  then
    do_curl "$(admin_url)/server-data/configuration" >/dev/null 2>&1
    local needs=$(pretty '.needsStorage // false' 2>/dev/null)
    local stores=$(pretty '.storeEvents // false' 2>/dev/null)
    local history=$(pretty '.storeEventsKeyHistory // false' 2>/dev/null)
    if [ "${needs}" = "true" ]
    then
      if [ "${stores}" != "true" ]
      then
        echo "Warning: server provisions require storage but it is disabled. Consider: server_data_configuration --keep-all"
        rc=1
      elif [ "${history}" != "true" ]
      then
        echo "Warning: server provisions require storage but key history is disabled. Consider: server_data_configuration --keep-all"
        rc=1
      fi
    fi
  fi

  if [ "${check_client}" = "true" ]
  then
    do_curl "$(admin_url)/client-data/configuration" >/dev/null 2>&1
    local needs=$(pretty '.needsStorage // false' 2>/dev/null)
    local stores=$(pretty '.storeEvents // false' 2>/dev/null)
    local history=$(pretty '.storeEventsKeyHistory // false' 2>/dev/null)
    if [ "${needs}" = "true" ]
    then
      if [ "${stores}" != "true" ]
      then
        echo "Warning: client provisions require storage but it is disabled. Consider: client_data_configuration --keep-all"
        rc=1
      elif [ "${history}" != "true" ]
      then
        echo "Warning: client provisions require storage but key history is disabled. Consider: client_data_configuration --keep-all"
        rc=1
      fi
    fi
  fi

  [ ${rc} -eq 0 ] && echo "Storage configuration is consistent."
  return ${rc}
}

# -----------------------------------------------------------------------------
# TRAFFIC CLIENT

client_data_configuration() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: client_data_configuration [-h|--help]; Manages agent client data configuration (gets current status by default)."
    echo "                                 [--discard-all]     ; Discards all the events processed."
    echo "                                 [--discard-history] ; Keeps only the last event processed for a key."
    echo "                                 [--keep-all]        ; Keeps all the events processed."
    echo "                                 [--disable-purge]   ; Skips events post-removal when a provision on 'purge' state is reached."
    echo "                                 [--enable-purge]    ; Processes events post-removal when a provision on 'purge' state is reached."
    return 0
  elif [ "$1" = "--discard-all" ]
  then
    do_curl -XPUT "$(admin_url)/client-data/configuration?discard=true&discardKeyHistory=true"
  elif [ "$1" = "--discard-history" ]
  then
    do_curl -XPUT "$(admin_url)/client-data/configuration?discard=false&discardKeyHistory=true"
  elif [ "$1" = "--keep-all" ]
  then
    do_curl -XPUT "$(admin_url)/client-data/configuration?discard=false&discardKeyHistory=false"
  elif [ "$1" = "--disable-purge" ]
  then
    do_curl -XPUT "$(admin_url)/client-data/configuration?disablePurge=true"
  elif [ "$1" = "--enable-purge" ]
  then
    do_curl -XPUT "$(admin_url)/client-data/configuration?disablePurge=false"
  else
    do_curl $(admin_url)/client-data/configuration
  fi
}

client_endpoint() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: client_endpoint [-h|--help] [--clean] [file]; Cleans/gets/updates current client endpoint configuration ($(admin_url)/client-endpoint)." && return 0
  [ -z "$1" ] && do_curl $(admin_url)/client-endpoint && return 0
  if [ "$1" = "--clean" ]
  then
    do_curl -XDELETE $(admin_url)/client-endpoint
  else
    do_curl -XPOST -d@${1} -H 'content-type:application/json' $(admin_url)/client-endpoint
  fi
}

client_provision() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: client_provision [-h|--help] [--clean]; Cleans/gets/configures current client provision configuration ($(admin_url)/client-provision)."
    echo "                                       [file]; Configure client provision by mean json specification."
    echo "                                   [id] [inState]; Filter provisions by id and optionally by inState."
    echo "                          [--dynamics] [period] [--wait]; Shows active dynamics with remaining sequences and ETA."
    echo "                                                 No period: single shot. Period <= 0: fastest. Period > 0: refresh interval."
    echo "                                                 --wait: blocking mode, keeps polling when no activity (waits for new triggers)."
    echo
    echo "  Examples:"
    echo "    client_provision                                      # List all provisions"
    echo "    client_provision --clean                              # Delete all provisions"
    echo "    client_provision my-provision.json                    # Configure from file"
    echo "    client_provision myFlow                               # Show provisions with id 'myFlow'"
    echo "    client_provision myFlow established                   # Show provision with id 'myFlow' and inState 'established'"
    echo "    client_provision --dynamics                           # Single shot dynamics"
    echo "    client_provision --dynamics 0                         # Fastest continuous"
    echo "    client_provision --dynamics 2                         # Refresh every 2s"
    echo "    client_provision --dynamics --wait                    # Fastest continuous, blocking"
    echo "    client_provision --dynamics 2 --wait                  # Refresh 2s, blocking"
    return 0
  fi

  [ -z "$1" ] && do_curl $(admin_url)/client-provision && return 0
  if [ "$1" = "--clean" ]
  then
    do_curl -XDELETE $(admin_url)/client-provision
  elif [ "$1" = "--dynamics" ]
  then
    shift
    local period= wait=false
    while [ $# -gt 0 ]; do
      case "$1" in
        --wait) wait=true ;;
        *) period=$1 ;;
      esac
      shift
    done
    local single_shot=false
    local waiting_dot=false
    [ -z "$period" ] && single_shot=true && period=0
    local start_epoch=$(date +%s)
    local initial_seqs=
    local prev_output=
    local empty_retries=0
    while true; do
      local json=$(curl -s --http2-prior-knowledge $(admin_url)/client-provision 2>/dev/null)
      local output=
      [ -n "$json" -a "$json" != "[]" ] && output=$(echo "$json" | jq -r '
        .[] | select(.dynamics and .dynamics.cps > 0 and .dynamics.sequenceEnd >= .dynamics.sequence) |
        .id as $id |
        .dynamics |
        (.sequenceEnd - .sequence) as $remaining |
        (($remaining / .cps) | floor) as $eta |
        "\($id): seq=\(.sequence)/\(.sequenceEnd) remaining=\($remaining) cps=\(.cps) repeat=\(.repeat) ETA=\($eta)s"
      ')
      if [ -n "$output" ]; then
        empty_retries=0
        # Capture initial sequences on first active iteration
        if [ -z "$initial_seqs" ]; then
          initial_seqs=$(echo "$json" | jq -r '
            [.[] | select(.dynamics and .dynamics.cps > 0 and .dynamics.sequenceEnd >= .dynamics.sequence) |
            {id: .id, seq: .dynamics.sequence, begin: .dynamics.sequenceBegin, end: .dynamics.sequenceEnd, cps: .dynamics.cps}] | @json
          ')
          start_epoch=$(date +%s)
        fi
        ${waiting_dot} && echo && waiting_dot=false
        if [ "$output" != "$prev_output" ]; then
          echo "$output"
          prev_output=$output
        fi
        ${single_shot} && break
        [ "$period" -gt 0 ] 2>/dev/null && sleep "$period"
      elif [ -n "$initial_seqs" ] && [ $empty_retries -lt 5 ]; then
        # Had active dynamics before — likely transient (admin busy or mid-update), retry
        empty_retries=$((empty_retries + 1))
        sleep 1
      elif ! ${wait}; then
        # Print summary if we tracked any dynamics
        if [ -n "$initial_seqs" ]; then
          local elapsed=$(( $(date +%s) - start_epoch ))
          [ $elapsed -lt 1 ] && elapsed=1
          local total_sent=$(echo "$initial_seqs" | jq -r "
            [.[] | .end - .seq + 1] | add // 0
          ")
          local initial_eta=$(echo "$initial_seqs" | jq -r '
            [.[] | select(.cps > 0) | ((.end - .begin + 1) / .cps)] | max | floor
          ')
          local avg_cps=$(( total_sent / elapsed ))
          echo
          echo "Expected duration: ${initial_eta}s | Elapsed: ${elapsed}s | Avg CPS: ${avg_cps}"
        else
          echo "No active dynamics."
        fi
        break
      else
        ${waiting_dot} || echo -n "No active dynamics."
        echo -n "."
        waiting_dot=true
        sleep 1
      fi
    done
  elif [ -f "$1" -o -p "$1" -o -r "$1" ]
  then
    do_curl -XPOST -d@${1} -H 'content-type:application/json' $(admin_url)/client-provision
  else
    local id=$1
    local inState=$2
    local filter=".[] | select(.id == \"${id}\")"
    [ -n "${inState}" ] && filter+=" | select(.inState == \"${inState}\")"
    do_curl $(admin_url)/client-provision >/dev/null && pretty | jq "[${filter}]"
  fi
}

client_provision_trigger() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: client_provision_trigger [-h|--help] <id> [sequenceBegin] [sequenceEnd] [cps] [repeat] [--in-state <state>]"
    echo "                               Triggers a client provision. Omitted params keep server-side values."
    echo
    echo "  Sequence iterator behavior:"
    echo "    - Providing sequenceBegin and/or sequenceEnd resets the iterator (fresh start)."
    echo "    - Providing only cps (no range) resumes from where it left off."
    echo "    - cps=0 (or omitted) pauses without losing position."
    echo "    - Providing range without cps prepares the range without starting (cps defaults to 0)."
    echo
    echo "  Examples:"
    echo "    client_provision_trigger myFlow                               # Sync trigger (sequence=0, inState=initial)"
    echo "    client_provision_trigger myFlow --in-state established        # Sync trigger with custom inState"
    echo "    client_provision_trigger myFlow 0 99999                       # Prepare range (paused, cps=0)"
    echo "    client_provision_trigger myFlow 0 99999 5000                  # Async trigger at 5000 cps"
    echo "    client_provision_trigger myFlow 0 99999 5000 true             # Async trigger with repeat"
    echo "    client_provision_trigger myFlow 0 99999 5000 --in-state step2 # Async trigger with custom inState"
    return 0
  fi

  [ -z "$1" ] && echo "Error: provision id required" && return 1

  local id=$1; shift
  local seqBegin= seqEnd= cps= repeat= inState=
  while [ $# -gt 0 ]; do
    case "$1" in
      --in-state) inState=$2; shift ;;
      *) [ -z "${seqBegin}" ] && seqBegin=$1 && shift && continue
         [ -z "${seqEnd}" ] && seqEnd=$1 && shift && continue
         [ -z "${cps}" ] && cps=$1 && shift && continue
         [ -z "${repeat}" ] && repeat=$1 && shift && continue
         ;;
    esac
    shift
  done
  local queryParams=
  [ -n "${seqBegin}" ] && queryParams+="&sequenceBegin=${seqBegin}"
  [ -n "${seqEnd}" ] && queryParams+="&sequenceEnd=${seqEnd}"
  [ -n "${cps}" ] && queryParams+="&cps=${cps}"
  [ -n "${repeat}" ] && queryParams+="&repeat=${repeat}"
  [ -n "${inState}" ] && queryParams+="&inState=${inState}"
  [ -n "${queryParams}" ] && queryParams=$(echo ${queryParams} | sed 's/&/?/')
  do_curl -XGET "$(admin_url)/client-provision/${id}${queryParams}"
}

client_provision_cps() {
  if [ "$1" = "-h" -o "$1" = "--help" -o $# -lt 2 ]
  then
    echo "Usage: client_provision_cps <id> <cps> [--ramp-up-time <seconds>] [--repeat true|false] [--in-state <state>]"
    echo "       Changes CPS (rate) for a running provision without resetting the sequence iterator."
    echo "       With --ramp-up-time, ramps linearly from current cps to target over the given seconds."
    echo "       Supports both ramp-up (increase) and ramp-down (decrease). cps=0 pauses."
    echo "       Default: ramp-up-time=0 (instant), in-state=initial."
    return 0
  fi
  local id=$1 cps=$2; shift 2
  local inState= repeat= rampUp=0
  while [ $# -gt 0 ]; do
    case "$1" in
      --in-state) inState=$2; shift ;;
      --repeat) repeat=$2; shift ;;
      --ramp-up-time) rampUp=$2; shift ;;
    esac
    shift
  done
  local base_q=
  [ -n "$inState" ] && base_q="inState=${inState}"
  [ -n "$repeat" ] && { [ -n "$base_q" ] && base_q+="&"; base_q+="repeat=${repeat}"; }

  # Build URL with cps: base_q may be empty
  _cps_url() { local c=$1; local q="${base_q:+${base_q}&}cps=${c}"; echo "$(admin_url)/client-provision/${id}?${q}"; }

  if [ "$(echo "${rampUp} > 0" | bc)" = "1" ]; then
    # Read current cps from provision dynamics
    do_curl -XGET "$(admin_url)/client-provision" >/dev/null 2>&1
    local from_cps=$(pretty "[.[] | select(.id==\"${id}\" and .inState==\"initial\")] | .[0].dynamics.cps // 0" 2>/dev/null || echo 0)
    [ -z "${from_cps}" ] && from_cps=0

    local step_ns=100000000 # 0.1s fixed
    local start_ns=$(date +%s%N)
    local ramp_ns=$(echo "${rampUp} * 1000000000 / 1" | bc)
    local prev_cps=-1
    local direction="ramp-up"
    [ "${cps}" -lt "${from_cps}" ] 2>/dev/null && direction="ramp-down"
    while true; do
      local now_ns=$(date +%s%N)
      local elapsed_ns=$((now_ns - start_ns))
      [ ${elapsed_ns} -ge ${ramp_ns} ] && break
      local cps_now=$(echo "${from_cps} + (${cps} - ${from_cps}) * ${elapsed_ns} / ${ramp_ns}" | bc)
      if [ "${cps_now}" != "${prev_cps}" ]; then
        do_curl -XGET "$(_cps_url ${cps_now})" >/dev/null
        prev_cps=${cps_now}
        local elapsed_s=$(echo "scale=1; ${elapsed_ns} / 1000000000" | bc)
        echo -ne "\r  ${direction}: ${cps_now} -> ${cps} cps (${elapsed_s}s/${rampUp}s)"
      fi
      local target_ns=$(( (elapsed_ns / step_ns + 1) * step_ns ))
      local sleep_ns=$((target_ns - ($(date +%s%N) - start_ns)))
      [ ${sleep_ns} -gt 0 ] && sleep $(echo "scale=6; ${sleep_ns} / 1000000000" | bc)
    done
    do_curl -XGET "$(_cps_url ${cps})" >/dev/null
    echo -e "\r  ${direction}: ${cps}/${cps} cps (${rampUp}s/${rampUp}s) - done    "
  else
    do_curl -XGET "$(_cps_url ${cps})"
  fi
}

client_provision_unused() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: client_provision_unused [-h|--help]; Get current client provision configuration still not used ($(admin_url)/client-provision/unused)." && return 0
  do_curl $(admin_url)/client-provision/unused && return 0
}

dispatch_latency() {
  if [ "$1" = "-h" -o "$1" = "--help" ]; then
    echo "Usage: dispatch_latency [-h|--help] [--json] [--watch]"
    echo "       Shows worker pool dispatch latency (queue wait + thread wakeup)."
    echo
    echo "       --watch        Refresh every second showing last-second avg and dispatch count."
    echo "       --json         Raw JSON output (single shot)."
    echo
    echo "       avgUs < 50     : pool idle (ideal)"
    echo "       avgUs 100-500  : busy but healthy"
    echo "       avgUs 1000-5000: at capacity (add --traffic-client-worker-threads)"
    echo "       avgUs > 10000  : saturated"
    return 0
  fi
  local url="$(admin_url)/client/dispatch-latency"
  if [ "$1" = "--json" ]; then
    curl -s --http2-prior-knowledge "$url" 2>/dev/null
    return
  fi
  _dl_status() {
    local avg=$1
    [ "$avg" -gt 10000 ] 2>/dev/null && echo "SATURATED" && return
    [ "$avg" -gt 1000 ] 2>/dev/null && echo "at capacity" && return
    [ "$avg" -gt 50 ] 2>/dev/null && echo "busy" && return
    echo "idle"
  }
  if [ "$1" = "--watch" ]; then
    local prev_sum=0 prev_count=0
    while true; do
      local json=$(curl -s --http2-prior-knowledge "$url" 2>/dev/null)
      [ -z "$json" ] && echo "No data" && sleep 1 && continue
      local sum=$(echo "$json" | jq -r '.avgUs * .count | floor')
      local count=$(echo "$json" | jq -r '.count')
      local d_sum=$((sum - prev_sum))
      local d_count=$((count - prev_count))
      if [ "$d_count" -gt 0 ] 2>/dev/null; then
        local d_avg=$((d_sum / d_count))
        echo "avg=${d_avg}us dispatches=${d_count}/s [$(_dl_status $d_avg)]"
      else
        echo "(no dispatches)"
      fi
      prev_sum=$sum prev_count=$count
      sleep 1
    done
  else
    local json=$(curl -s --http2-prior-knowledge "$url" 2>/dev/null)
    [ -z "$json" ] && echo "No data (is h2agent running?)" && return 1
    local avg=$(echo "$json" | jq -r '.avgUs | floor')
    local max=$(echo "$json" | jq -r '.maxUs')
    local count=$(echo "$json" | jq -r '.count')
    echo "Dispatch latency: avg=${avg}us max=${max}us count=${count} [$(_dl_status $avg)]"
  fi
  unset -f _dl_status
}

client_data() {
  local clean=
  local surf=
  local dump=

  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: client_data [-h|--help]; Inspects client data events ($(admin_url)/client-data)."
    echo "                   [client endpoint id] [method] [uri] [[-]event number] [event path] ; Positional filters to narrow the client data selection."
    echo "                                                                                        Event number may be negative to access by reverse chronological order."
    echo "                   [--summary] [max keys]          ; Gets current client data summary to guide further queries."
    echo "                                                     Displayed keys (client endpoint id/method/uri) could be limited (10 by default, -1: no limit)."
    echo "                   [--clean] [query filters]       ; Removes client data events."
    echo "                   [--surf] [query filters]        ; Interactive sorted (regardless endpoint/method/uri) client data navigation."
    echo "                   [--dump] [query filters]        ; Dumps all sequences detected for client data under 'client-data-sequences' directory."
    return 0

  elif [ "$1" = "--summary" ]
  then
    local maxKeys=${2:-10}
    local queryParams=
    [ -n "${maxKeys}" ] && queryParams="?maxKeys=${maxKeys}"
    [ "${maxKeys}" = "-1" ] && queryParams=
    do_curl "$(admin_url)/client-data/summary${queryParams}"
    return 0
  elif [ "$1" = "--clean" ]
  then
    clean=yes
    shift
  elif [ "$1" = "--surf" ]
  then
    surf=yes
    shift
  elif [ "$1" = "--dump" ]
  then
    dump=yes
    shift
  fi

  local clientEndpointId=$1
  local requestMethod=$2
  local requestUri=$3
  local eventNumber=$4
  local eventPath=$5

  local keyIndicators=""
  [ -n "${clientEndpointId}" ] && keyIndicators+="x"
  [ -n "${requestMethod}" ] && keyIndicators+="x"
  [ -n "${requestUri}" ] && keyIndicators+="x"
  [ "${keyIndicators}" != "" -a "${keyIndicators}" != "xxx" ] && echo "Error: client endpoint id & method & uri must be provided" && return 1
  [ -n "${eventNumber}" -a -z "${keyIndicators}" ] && echo "Error: client endpoint id/method/uri are also required when eventNumber is provided" && return 1
  [ -n "${eventPath}" -a -z "${eventNumber}" ] && echo "Error: eventNumber is required when eventPath is provided" && return 1

  local queryParams=
  [ -n "${clientEndpointId}" ] && queryParams="?clientEndpointId=${clientEndpointId}"
  [ -n "${requestMethod}" ] && queryParams="${queryParams}&requestMethod=${requestMethod}"

  local curl_method=
  [ -n "${clean}" ] && curl_method="-XDELETE"

  local devnull=
  [ -n "${dump}" -o -n "${surf}" ] && devnull=">/dev/null"

  if [ -n "${requestUri}" ]
  then
    local urlencode=
    [ -n "${requestUri}" ] && urlencode="--data-urlencode requestUri=\"${requestUri}\""
    [ -n "${eventNumber}" ] && urlencode+=" --data-urlencode eventNumber=${eventNumber}"
    [ -n "${eventPath}" ] && urlencode+=" --data-urlencode eventPath=${eventPath}"
    eval do_curl ${curl_method} -G ${urlencode} "$(admin_url)/client-data${queryParams}" ${devnull}
  else
    if [ -z "${clean}${dump}${surf}" ]
    then
      echo
      echo "Take care about storage size when querying without filters."
      echo "You may want to check the summary before: client_data --summary"
      echo
      echo "Press ENTER to continue, CTRL-C to abort ..."
      read -r dummy
    fi
    eval do_curl ${curl_method} "$(admin_url)/client-data" ${devnull}
  fi

  [ -z "${clean}${dump}${surf}" ] && return 0

  local sequences=
  if [ -n "${requestUri}" ]; then sequences=( $(pretty ".events[].sendseq" | sort -n) ) ; else sequences=( $(pretty ".[].events[].sendseq" | sort -n) ) ; fi
  local indx_max=${#sequences[@]}

  if [ -n "${dump}" ]
  then
    mkdir -p client-data-sequences
    local arrayPrefix=
    [ -z "${requestUri}" ] && arrayPrefix=".[] | "
    for s in ${sequences[@]}; do pretty | jq "${arrayPrefix}select (.events[].sendseq == $s) | del (.events[] | select (.sendseq != $s))" > client-data-sequences/$(printf "%02d\n" "$s").json ; done

  elif [ -n "${surf}" ]
  then
    if [ -n "${requestUri}" ]
    then
      pretty | jq '.method as $method | .uri as $uri | .events | map({events: [.], method: $method, uri: $uri})' > ${_H2A_CURL_OUT}.sorted
    else
      pretty | jq 'map(. as $parent | .events |= sort_by(.sendseq) | .events[] | {events: [.], method: $parent.method, uri: $parent.uri}) | sort_by(.events[].sendseq)' > ${_H2A_CURL_OUT}.sorted
    fi

    local indx=0
    [ ! -s ${_H2A_CURL_OUT}.sorted ] && echo && cat ${_H2A_CURL_OUT} && return 0
    while true
    do
      echo -e "\nMessage $((indx+1)):\n"
      jq ".[$indx]" ${_H2A_CURL_OUT}.sorted
      indx=$((indx+1))
      [ $indx -eq $indx_max ] && echo -e "\n\nThat's all ! ($indx_max events)\n" && return 0
      echo -e "\n\nPress ENTER to print next sequence ..."
      read -r dummy
    done
  fi
}

# -----------------------------------------------------------------------------
# OPERATION SCHEMAS

schema_schema() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: schema_schema [-h|--help]; Gets the schema configuration schema ($(admin_url)/schema/schema)." && return 0
  do_curl "$(admin_url)/schema/schema"
}

vault_schema() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: vault_schema [-h|--help]; Gets the agent vault configuration schema ($(admin_url)/vault/schema)." && return 0
  do_curl "$(admin_url)/vault/schema"
}

server_matching_schema() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: server_matching_schema [-h|--help]; Gets the server matching configuration schema ($(admin_url)/server-matching/schema)." && return 0
  do_curl "$(admin_url)/server-matching/schema"
}

server_provision_schema() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: server_provision_schema [-h|--help]; Gets the server provision configuration schema ($(admin_url)/server-provision/schema)." && return 0
  do_curl "$(admin_url)/server-provision/schema"
}

client_endpoint_schema() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: client_endpoint_schema [-h|--help]; Gets the client endpoint configuration schema ($(admin_url)/client-endpoint/schema)." && return 0
  do_curl "$(admin_url)/client-endpoint/schema"
}

client_provision_schema() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: client_provision_schema [-h|--help]; Gets the client provision configuration schema ($(admin_url)/client-provision/schema)." && return 0
  do_curl "$(admin_url)/client-provision/schema"
}

# -----------------------------------------------------------------------------
# BLOCKING WAIT (LONG-POLL)

wait_vault() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: wait_vault [-h|--help] <key> [timeoutMs] [--value <value>]"
    echo "       Blocks until vault entry <key> changes (or equals <value>), or timeout."
    echo "       Without --value: waits for any change from current value."
    echo "       Default timeoutMs: 30000 (30s)."
    echo
    echo "  Examples:"
    echo "    wait_vault MY_FLAG                     # Wait for any change"
    echo "    wait_vault MY_FLAG 60000               # Wait 60s for any change"
    echo "    wait_vault MY_FLAG 30000 --value done  # Wait until value is 'done'"
    return 0
  fi

  [ -z "$1" ] && echo "Error: key required" && return 1
  local key=$1; shift
  local timeoutMs=30000 value=
  while [ $# -gt 0 ]; do
    case "$1" in
      --value) value=$2; shift ;;
      *) timeoutMs=$1 ;;
    esac
    shift
  done
  local maxTime=$(( (timeoutMs / 1000) + 1 ))
  local q="timeoutMs=${timeoutMs}"
  [ -n "${value}" ] && q+="&value=${value}"

  ${CURL} --max-time ${maxTime} "$(admin_url)/vault/${key}/wait?${q}"
  local rc=$?
  echo
  return ${rc}
}

# -----------------------------------------------------------------------------
# AUXILIARY

pretty() {
  local jq_expr=${1:-.}
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: pretty [-h|--help]; Beautifies json content for last operation response."
    echo "              [jq expression, '.' by default]; jq filter over previous content."
    echo "              Example filter: schema && pretty '.[] | select(.id==\"myRequestsSchema\")'"
    return 0
  fi
  tail -n -1 ${_H2A_CURL_OUT} | jq "${jq_expr}"
}

raw() {
  local jq_expr=${1:-.}
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: raw [-h|--help]; Outputs raw json content for last operation response."
    echo "           [jq expression, '.' by default]; jq filter over previous content."
    echo "           Example filter: schema && raw '.[] | select(.id==\"myRequestsSchema\")'"
    return 0
  fi
  tail -n -1 ${_H2A_CURL_OUT} | jq -c "${jq_expr}"
}

trace() {
  local level=$1
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: trace [-h|--help] [level: Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency]; Gets/sets h2agent tracing level."
    return 0
  fi
  if [ -n "$1" ]
  then
    echo -n "Level selected: ${level}"
    local recommended_level="Warning"
    [ "$1" != "${recommended_level}" ] && echo -n " (recommended '${recommended_level}' for normal operation)."
    echo
    PLAIN=true do_curl -XPUT $(admin_url)/logging?level=${level}
  else
    PLAIN=true do_curl -XGET $(admin_url)/logging
  fi
}

metrics() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: metrics [-h|--help]; Prometheus metrics." && return 0
  curl -s $(metrics_url)
}

traffic_summary() {
  local snap_dir="/tmp/.h2agent_traffic_summaries"

  # Colors (disabled if not a terminal)
  local C_RST="" C_BLD="" C_GRN="" C_RED="" C_CYN="" C_YLW=""
  if [ -t 1 ]; then
    C_RST='\033[0m'; C_BLD='\033[1m'; C_GRN='\033[32m'
    C_RED='\033[31m'; C_CYN='\033[36m'; C_YLW='\033[33m'
  fi

  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: traffic_summary [-h|--help] [--show] [--clean] [--json] [<ref1> <ref2>]"
    echo "       Traffic counter summary from prometheus metrics."
    echo
    echo "       (no args)          Save a counters snapshot (auto timestamp)."
    echo "       --save <label>     Save a snapshot with a label."
    echo "       --show             List saved snapshots with labels."
    echo "       --now              Snapshot + delta from zeroed (*) to current."
    echo "       <ref> --now        Snapshot + delta from <ref> to current."
    echo "       <ref1> <ref2>      Delta between two refs (timestamp or label)."
    echo "       --json             JSON output (combinable with delta or --now)."
    echo "       --clean            Remove all saved snapshots."
    echo
    echo "       References can be unix timestamps or labels (order does not matter)."
    echo "       (*) Reserved label 'zeroed': virtual zero-baseline snapshot."
    echo
    echo "       Snapshots: ${snap_dir}/counters.<unix_ts>"
    echo "       Labels:    ${snap_dir}/labels (label=timestamp mapping)"
    echo
    echo "       Workflow: traffic_summary --save before  # labeled baseline"
    echo "                 # ... run your test ..."
    echo "                 traffic_summary --save after   # labeled post-test"
    echo "                 traffic_summary --show         # list snapshots"
    echo "                 traffic_summary before after   # delta by label"
    echo "                 traffic_summary zeroed after   # absolute values"
    echo "                 traffic_summary before after --json  # JSON output"
    echo "                 traffic_summary --now          # shortcut: zeroed -> current"
    echo "                 traffic_summary before --now   # shortcut: before -> current"
    echo "                 traffic_summary --now --json   # JSON output"
    return 0
  fi

  if [ "$1" = "--clean" ]
  then
    rm -rf "${snap_dir}"
    echo "Snapshots removed."
    return 0
  fi

  mkdir -p "${snap_dir}"
  touch "${snap_dir}/labels"

  # Resolve a reference (label or timestamp) to "filepath timestamp".
  _resolve_ref() {
    local ref=$1
    if [ "$ref" = "zeroed" ]; then
      echo "/dev/null 0"
      return 0
    fi
    # Try as label first
    local ts=$(awk -F= -v l="$ref" '$1==l{print $2;exit}' "${snap_dir}/labels")
    if [ -n "$ts" ] && [ -f "${snap_dir}/counters.${ts}" ]; then
      echo "${snap_dir}/counters.${ts} ${ts}"
      return 0
    fi
    # Try as timestamp
    if [ -f "${snap_dir}/counters.${ref}" ]; then
      echo "${snap_dir}/counters.${ref} ${ref}"
      return 0
    fi
    return 1
  }

  # Reverse-lookup: timestamp to label (if any)
  _label_for_ts() {
    awk -F= -v t="$1" '$2==t{print $1;exit}' "${snap_dir}/labels"
  }

  # Format a reference for display: "timestamp (label) date"
  _fmt_ref() {
    local ts=$1
    if [ "$ts" = "0" ]; then
      printf "${C_CYN}zeroed${C_RST}"
      return
    fi
    local lbl=$(_label_for_ts "$ts")
    local dt=$(date -d @${ts} '+%Y-%m-%d %H:%M:%S' 2>/dev/null)
    if [ -n "$lbl" ]; then
      printf "${C_CYN}%s${C_RST} (%s) %s" "$ts" "$lbl" "$dt"
    else
      printf "${C_CYN}%s${C_RST} %s" "$ts" "$dt"
    fi
  }

  # Show history
  if [ "$1" = "--show" ]
  then
    local files=$(ls -1 "${snap_dir}"/counters.* 2>/dev/null | sort)
    if [ -z "$files" ]; then
      echo "No snapshots saved yet."
      unset -f _resolve_ref _label_for_ts _fmt_ref
      return 0
    fi
    printf "${C_BLD}%-14s  %-12s  %s${C_RST}\n" "TIMESTAMP" "LABEL" "DATE/TIME"
    for f in ${files}; do
      local t=${f##*.}
      local lbl=$(_label_for_ts "$t")
      printf "%-14s  %-12s  %s\n" "${t}" "${lbl:--}" "$(date -d @${t} '+%Y-%m-%d %H:%M:%S')"
    done
    unset -f _resolve_ref _label_for_ts _fmt_ref
    return 0
  fi

  # Shortcut: snapshot + delta to current
  # --now              → zeroed → current
  # <ref> --now        → ref → current
  # --now --json       → zeroed → current (JSON)
  # <ref> --now --json → ref → current (JSON)
  local now_mode=false now_ref="zeroed" now_json=""
  local now_args=()
  for a in "$@"; do
    case "$a" in
      --now) now_mode=true ;;
      --json) now_json="--json" ;;
      *) now_args+=("$a") ;;
    esac
  done
  if $now_mode; then
    [ ${#now_args[@]} -gt 0 ] && now_ref="${now_args[0]}"
    # Save ephemeral snapshot (no label, just timestamp)
    traffic_summary
    local latest=$(ls -1t "${snap_dir}"/counters.* 2>/dev/null | head -1)
    local latest_ts=${latest##*.}
    traffic_summary "${now_ref}" "${latest_ts}" ${now_json}
    return $?
  fi

  # Save snapshot (with optional label)
  if [ $# -eq 0 ] || [ "$1" = "--save" ]
  then
    local label=""
    if [ "$1" = "--save" ]; then
      label=$2
      if [ -z "$label" ]; then
        echo "Usage: traffic_summary --save <label>"
        unset -f _resolve_ref _label_for_ts _fmt_ref
        return 1
      fi
      if [ "$label" = "zeroed" ]; then
        echo "Error: 'zeroed' is a reserved label."
        unset -f _resolve_ref _label_for_ts _fmt_ref
        return 1
      fi
    fi
    local m=$(curl -s $(metrics_url))
    if [ -z "$m" ]; then
      echo "No metrics available (is h2agent running?)"
      unset -f _resolve_ref _label_for_ts _fmt_ref
      return 1
    fi
    local ts=$(date +%s)
    echo "$m" | grep -E '_counter\{|_gauge\{|_bucket\{|_sum\{|_count\{' > "${snap_dir}/counters.${ts}"
    if [ -n "$label" ]; then
      # Remove old mapping for this label if exists
      sed -i "/^${label}=/d" "${snap_dir}/labels"
      echo "${label}=${ts}" >> "${snap_dir}/labels"
      echo -e "Snapshot saved: $(_fmt_ref "$ts")"
    else
      echo -e "Snapshot saved: $(_fmt_ref "$ts")"
    fi
    unset -f _resolve_ref _label_for_ts _fmt_ref
    return 0
  fi

  # Delta mode: need at least 2 refs
  local json=false
  local args=()
  for a in "$@"; do
    [ "$a" = "--json" ] && json=true || args+=("$a")
  done

  if [ ${#args[@]} -ne 2 ]; then
    traffic_summary -h
    unset -f _resolve_ref _label_for_ts _fmt_ref
    return 1
  fi

  local resolved1=$(_resolve_ref "${args[0]}")
  [ -z "$resolved1" ] && echo "Reference '${args[0]}' not found." && unset -f _resolve_ref _label_for_ts _fmt_ref && return 1
  local f1=${resolved1% *} ts1=${resolved1##* }

  local resolved2=$(_resolve_ref "${args[1]}")
  [ -z "$resolved2" ] && echo "Reference '${args[1]}' not found." && unset -f _resolve_ref _label_for_ts _fmt_ref && return 1
  local f2=${resolved2% *} ts2=${resolved2##* }

  # Silent swap: counters always grow, so ensure older snapshot is first
  if [ "$ts1" -gt "$ts2" ] 2>/dev/null; then
    local tmp_f=$f1 tmp_ts=$ts1
    f1=$f2; ts1=$ts2; f2=$tmp_f; ts2=$tmp_ts
  fi

  # Counter delta helper: f2 minus f1
  _tc() {
    local pattern=$1
    local v2=$(awk "/${pattern}/{s+=\$2}END{print s+0}" "$f2")
    local v1=$(awk "/${pattern}/{s+=\$2}END{print s+0}" "$f1")
    echo $((v2 - v1))
  }

  # Float delta helper (for histogram _sum)
  _tf() {
    local pattern=$1
    local v2=$(awk "/${pattern}/{s+=\$2}END{printf \"%.9f\", s}" "$f2")
    local v1=$(awk "/${pattern}/{s+=\$2}END{printf \"%.9f\", s}" "$f1")
    awk "BEGIN{printf \"%.9f\", ${v2} - ${v1}}"
  }

  # Gauge stats from f2: prints "min avg max" (floats)
  _gauge_stats() {
    local pattern=$1
    awk "/${pattern}/{v=\$2;n++;s+=v;if(n==1||v<mn)mn=v;if(n==1||v>mx)mx=v}END{if(n>0)printf \"%.6f %.6f %.6f\",mn,s/n,mx;else print \"- - -\"}" "$f2"
  }

  # Percentile from histogram buckets (linear interpolation like Prometheus histogram_quantile)
  # Usage: _hpct <metric_prefix> <p1> [p2] [p3] ...
  # Prints space-separated percentile values
  _hpct() {
    local prefix=$1; shift
    awk -v prefix="$prefix" -v pcts="$*" -v file1="$f1" '
    BEGIN { np = split(pcts, P, " ") }
    FILENAME==file1 {
      if (index($0, prefix "_bucket{")) {
        match($0, /le="[^"]*"/); le = substr($0, RSTART+4, RLENGTH-5)
        f1[le] += $2
      }
      next
    }
    {
      if (index($0, prefix "_bucket{")) {
        match($0, /le="[^"]*"/); le = substr($0, RSTART+4, RLENGTH-5)
        f2[le] += $2
      }
    }
    END {
      n = 0
      for (le in f2) {
        d = (f2[le]+0) - (f1[le]+0)
        if (le == "+Inf") { total = d; continue }
        n++; bnd[n] = le+0; cnt[le+0] = d
      }
      for (i=1;i<=n;i++) for (j=i+1;j<=n;j++) if(bnd[i]>bnd[j]){t=bnd[i];bnd[i]=bnd[j];bnd[j]=t}
      if (total+0 <= 0) { for (i=1;i<=np;i++) printf "N/A "; print ""; exit }
      for (pi=1;pi<=np;pi++) {
        tgt = P[pi]/100 * total; prev_le=0; prev_cnt=0; res="N/A"
        for (i=1;i<=n;i++) {
          if (cnt[bnd[i]] >= tgt) {
            denom = cnt[bnd[i]] - prev_cnt
            res = (denom>0) ? prev_le + (bnd[i]-prev_le)*(tgt-prev_cnt)/denom : bnd[i]
            break
          }
          prev_le=bnd[i]; prev_cnt=cnt[bnd[i]]
        }
        printf "%s ", res
      }
      print ""
    }' "$f1" "$f2"
  }

  # Per-label latency breakdown (source, method, status_code)
  # Outputs pipe-separated: source|method|status_code|avg_s|gauge_s|count
  _latency_by_label() {
    local prefix=$1
    awk -v prefix="$prefix" -v file1="$f1" '
    function xlab(s, name,    p, i, v) {
      p = name "=\""
      if ((i = index(s, p)) > 0) { v = substr(s, i+length(p)); return substr(v, 1, index(v, "\"")-1) }
      return ""
    }
    {
      if (index($0, prefix "_sum{")) t = "sum"
      else if (index($0, prefix "_count{")) t = "count"
      else if (index($0, prefix "_gauge{")) t = "gauge"
      else next
      match($0, /\{[^}]*\}/); lb = substr($0, RSTART+1, RLENGTH-2)
      if (FILENAME==file1) a1[t,lb]+=$NF; else { a2[t,lb]+=$NF; seen[lb]=1 }
    }
    END {
      for (lb in seen) {
        dc = (a2["count",lb]+0) - (a1["count",lb]+0)
        ds = (a2["sum",lb]+0) - (a1["sum",lb]+0)
        g = a2["gauge",lb]+0
        if (dc > 0 || g > 0) {
          avg = (dc>0) ? ds/dc : 0
          printf "%s|%s|%s|%.6f|%.6f|%d\n", xlab(lb,"source"), xlab(lb,"method"), xlab(lb,"status_code"), avg, g, dc
        }
      }
    }' "$f1" "$f2" | sort -t'|' -k1,1 -k2,2 -k3,3n
  }

  # --- JSON output ---
  if $json; then
    local j='{'
    j+="\"from\":\"${ts1}\",\"to\":\"${ts2}\","

    if grep -q '^h2agent_traffic_server_' "$f2" 2>/dev/null; then
      local srv_accepted=$(_tc '^h2agent_traffic_server_observed_requests_accepted_counter\{')
      local srv_errored=$(_tc '^h2agent_traffic_server_observed_requests_errored_counter\{')
      local srv_responses=$(_tc '^h2agent_traffic_server_observed_responses_counter\{')
      local srv_prov_ok=$(_tc '^h2agent_traffic_server_provisioned_requests_counter\{.*result="successful"')
      local srv_prov_fail=$(_tc '^h2agent_traffic_server_provisioned_requests_counter\{.*result="failed"')
      local srv_purge_ok=$(_tc '^h2agent_traffic_server_purged_contexts_counter\{.*result="successful"')
      local srv_purge_fail=$(_tc '^h2agent_traffic_server_purged_contexts_counter\{.*result="failed"')
      j+="\"server\":{\"requests_accepted\":${srv_accepted},\"requests_errored\":${srv_errored},"
      j+="\"responses\":${srv_responses},"
      # response codes
      j+="\"response_codes\":{"
      local codes=$(grep '^h2agent_traffic_server_observed_responses_counter{' "$f2" | sed 's/.*status_code="\([^"]*\)".*/\1/' | sort -u)
      local first=true
      for code in ${codes}; do
        local count=$(_tc "^h2agent_traffic_server_observed_responses_counter\{.*status_code=\"${code}\"")
        if [ $count -gt 0 ]; then
          $first || j+=","
          j+="\"${code}\":${count}"
          first=false
        fi
      done
      j+="},"
      j+="\"provisioned_ok\":${srv_prov_ok},\"provisioned_fail\":${srv_prov_fail},"
      j+="\"purged_ok\":${srv_purge_ok},\"purged_fail\":${srv_purge_fail}},"
    fi

    if grep -q '^h2agent_traffic_client_' "$f2" 2>/dev/null; then
      local sent=$(_tc '^h2agent_traffic_client_observed_requests_sents_counter\{')
      local unsent=$(_tc '^h2agent_traffic_client_observed_requests_unsent_counter\{')
      local timedout=$(_tc '^h2agent_traffic_client_observed_responses_timedout_counter\{')
      local jc_fail=$(_tc '^h2agent_traffic_client_response_validation_failures_counter\{')
      local rx_2xx=$(_tc '^h2agent_traffic_client_observed_responses_received_counter\{.*status_code="2')
      local rx_3xx=$(_tc '^h2agent_traffic_client_observed_responses_received_counter\{.*status_code="3')
      local rx_4xx=$(_tc '^h2agent_traffic_client_observed_responses_received_counter\{.*status_code="4')
      local rx_5xx=$(_tc '^h2agent_traffic_client_observed_responses_received_counter\{.*status_code="5')
      local received=$((rx_2xx + rx_3xx + rx_4xx + rx_5xx))
      local failed=$((unsent + timedout + rx_4xx + rx_5xx))
      local total=$((sent + unsent))
      local verdict="NO_DATA" pass_pct="0.00"
      if [ $failed -eq 0 ] && [ $total -gt 0 ]; then
        verdict="PASS"; pass_pct="100.00"
      elif [ $total -gt 0 ]; then
        verdict="FAIL"; pass_pct=$(awk "BEGIN{printf \"%.2f\", ($total - $failed) * 100 / $total}")
      fi
      j+="\"client\":{\"sent\":${sent},\"unsent\":${unsent},"
      j+="\"received\":${received},\"rx_2xx\":${rx_2xx},\"rx_3xx\":${rx_3xx},\"rx_4xx\":${rx_4xx},\"rx_5xx\":${rx_5xx},"
      j+="\"timedout\":${timedout},\"jc_failures\":${jc_fail},"
      j+="\"total\":${total},\"failed\":${failed},"
      j+="\"verdict\":\"${verdict}\",\"pass_pct\":${pass_pct}"
      # Latency
      local _lp="h2agent_traffic_client_responses_delay_seconds"
      local jlat=""
      if grep -q "^${_lp}_gauge{" "$f2" 2>/dev/null; then
        local gs=$(_gauge_stats "^${_lp}_gauge\{")
        local g_min=${gs%% *}; local g_rest=${gs#* }; local g_avg=${g_rest%% *}; local g_max=${g_rest#* }
        jlat+="\"gauge_min\":${g_min},\"gauge_avg\":${g_avg},\"gauge_max\":${g_max},"
      fi
      if grep -q "^${_lp}_count{" "$f2" 2>/dev/null; then
        local d_sum=$(_tf "^${_lp}_sum\{")
        local d_count=$(_tc "^${_lp}_count\{")
        if [ "$d_count" -gt 0 ] 2>/dev/null; then
          local avg_lat=$(awk "BEGIN{printf \"%.9f\", ${d_sum}/${d_count}}")
          jlat+="\"avg\":${avg_lat},"
        fi
        local pcts=$(_hpct "$_lp" 50 90 99)
        local p50=${pcts%% *}; local pcts_rest=${pcts#* }; local p90=${pcts_rest%% *}; local p99=${pcts_rest#* }
        p99=${p99%% *}
        [ "$p50" != "N/A" ] && jlat+="\"p50\":${p50},\"p90\":${p90},\"p99\":${p99},"
      fi
      # Per-label breakdown
      local breakdown=$(_latency_by_label "$_lp")
      if [ -n "$breakdown" ]; then
        jlat+="\"by_label\":["
        local first_bl=true
        while IFS='|' read -r src mth sc avg g cnt; do
          $first_bl || jlat+=","
          jlat+="{\"source\":\"${src}\",\"method\":\"${mth}\",\"status_code\":\"${sc}\","
          jlat+="\"avg\":${avg},\"gauge\":${g},\"count\":${cnt}}"
          first_bl=false
        done <<< "$breakdown"
        jlat+="],"
      fi
      if [ -n "$jlat" ]; then
        j+=",\"latency\":{${jlat%,}}"
      fi
      j+="},"
    fi

    # Remove trailing comma and close
    j="${j%,}}"
    echo "$j" | python3 -m json.tool 2>/dev/null || echo "$j"
    unset -f _tc _tf _gauge_stats _hpct _latency_by_label _resolve_ref _label_for_ts _fmt_ref
    return 0
  fi

  # --- Table output ---
  echo -e "${C_BLD}=== Traffic Summary ===${C_RST}"
  echo -e "From: $(_fmt_ref "$ts1")"
  echo -e "To:   $(_fmt_ref "$ts2")"
  echo

  # Server (auto-detect)
  if grep -q '^h2agent_traffic_server_' "$f2" 2>/dev/null
  then
    local srv_accepted=$(_tc '^h2agent_traffic_server_observed_requests_accepted_counter\{')
    local srv_errored=$(_tc '^h2agent_traffic_server_observed_requests_errored_counter\{')
    local srv_responses=$(_tc '^h2agent_traffic_server_observed_responses_counter\{')
    local srv_prov_ok=$(_tc '^h2agent_traffic_server_provisioned_requests_counter\{.*result="successful"')
    local srv_prov_fail=$(_tc '^h2agent_traffic_server_provisioned_requests_counter\{.*result="failed"')
    local srv_purge_ok=$(_tc '^h2agent_traffic_server_purged_contexts_counter\{.*result="successful"')
    local srv_purge_fail=$(_tc '^h2agent_traffic_server_purged_contexts_counter\{.*result="failed"')

    echo -e "${C_BLD}--- Server ---${C_RST}"
    printf "  %-14s %s accepted, %s errored\n" "Requests:" "${srv_accepted}" "${srv_errored}"
    printf "  %-14s %s\n" "Responses:" "${srv_responses}"

    local codes=$(grep '^h2agent_traffic_server_observed_responses_counter{' "$f2" | sed 's/.*status_code="\([^"]*\)".*/\1/' | sort -u)
    for code in ${codes}; do
      local count=$(_tc "^h2agent_traffic_server_observed_responses_counter\{.*status_code=\"${code}\"")
      [ $count -gt 0 ] && printf "    %-12s %s\n" "${code}:" "${count}"
    done

    printf "  %-14s %s ok / %s failed\n" "Provisioned:" "${srv_prov_ok}" "${srv_prov_fail}"
    printf "  %-14s %s ok / %s failed\n" "Purged:" "${srv_purge_ok}" "${srv_purge_fail}"
    echo
  fi

  # Client (auto-detect) + verdict
  if grep -q '^h2agent_traffic_client_' "$f2" 2>/dev/null
  then
    local sent=$(_tc '^h2agent_traffic_client_observed_requests_sents_counter\{')
    local unsent=$(_tc '^h2agent_traffic_client_observed_requests_unsent_counter\{')
    local timedout=$(_tc '^h2agent_traffic_client_observed_responses_timedout_counter\{')
    local jc_fail=$(_tc '^h2agent_traffic_client_response_validation_failures_counter\{')
    local rx_2xx=$(_tc '^h2agent_traffic_client_observed_responses_received_counter\{.*status_code="2')
    local rx_3xx=$(_tc '^h2agent_traffic_client_observed_responses_received_counter\{.*status_code="3')
    local rx_4xx=$(_tc '^h2agent_traffic_client_observed_responses_received_counter\{.*status_code="4')
    local rx_5xx=$(_tc '^h2agent_traffic_client_observed_responses_received_counter\{.*status_code="5')

    local received=$((rx_2xx + rx_3xx + rx_4xx + rx_5xx))
    local failed=$((unsent + timedout + rx_4xx + rx_5xx))
    local total=$((sent + unsent))

    echo -e "${C_BLD}--- Client ---${C_RST}"
    printf "  %-14s %s\n" "Sent:" "${sent}"
    printf "  %-14s %s\n" "Unsent:" "${unsent}"
    printf "  %-14s %s (2xx:${C_GRN}%s${C_RST} 3xx:%s 4xx:${C_YLW}%s${C_RST} 5xx:${C_RED}%s${C_RST})\n" \
      "Received:" "${received}" "${rx_2xx}" "${rx_3xx}" "${rx_4xx}" "${rx_5xx}"
    printf "  %-14s %s\n" "Timeouts:" "${timedout}"
    [ $jc_fail -gt 0 ] && printf "  %-14s %s\n" "JC failures:" "${jc_fail}"
    echo -e "  ${C_BLD}---${C_RST}"
    if [ $failed -eq 0 ] && [ $total -gt 0 ]; then
      echo -e "  ${C_GRN}${C_BLD}PASS: 100% (${total}/${total})${C_RST}"
    elif [ $total -gt 0 ]; then
      local pass_pct=$(awk "BEGIN{printf \"%.2f\", ($total - $failed) * 100 / $total}")
      echo -e "  ${C_RED}${C_BLD}FAIL: ${pass_pct}% pass ($((total - failed))/${total}, ${failed} failed)${C_RST}"
    else
      echo -e "  ${C_YLW}NO DATA${C_RST}"
    fi

    # Latency
    local _lp="h2agent_traffic_client_responses_delay_seconds"
    local has_gauge=false has_hist=false
    grep -q "^${_lp}_gauge{" "$f2" 2>/dev/null && has_gauge=true
    grep -q "^${_lp}_count{" "$f2" 2>/dev/null && has_hist=true

    if $has_gauge || $has_hist; then
      echo
      echo -e "  ${C_BLD}Latency (s):${C_RST}"
      if $has_gauge; then
        local gs=$(_gauge_stats "^${_lp}_gauge\{")
        local g_min=${gs%% *}; local g_rest=${gs#* }; local g_avg=${g_rest%% *}; local g_max=${g_rest#* }
        printf "    %-12s min=%s  avg=%s  max=%s\n" "Gauge:" "$g_min" "$g_avg" "$g_max"
      fi
      if $has_hist; then
        local d_sum=$(_tf "^${_lp}_sum\{")
        local d_count=$(_tc "^${_lp}_count\{")
        if [ "$d_count" -gt 0 ] 2>/dev/null; then
          local avg_lat=$(awk "BEGIN{printf \"%.6f\", ${d_sum}/${d_count}}")
          printf "    %-12s %s  (sum=%s count=%s)\n" "Average:" "$avg_lat" "$d_sum" "$d_count"
        fi
        local pcts=$(_hpct "$_lp" 50 90 99)
        local p50=${pcts%% *}; local pcts_rest=${pcts#* }; local p90=${pcts_rest%% *}; local p99=${pcts_rest#* }
        p99=${p99%% *}
        [ "$p50" != "N/A" ] && printf "    %-12s p50=%s  p90=%s  p99=%s\n" "Percentile:" "$p50" "$p90" "$p99"
      fi
      # Per-label breakdown
      local breakdown=$(_latency_by_label "$_lp")
      if [ -n "$breakdown" ]; then
        echo -e "    ${C_BLD}By label:${C_RST}"
        printf "      ${C_BLD}%-28s %-6s %-6s %-12s %-12s %s${C_RST}\n" "SOURCE" "METHOD" "STATUS" "AVG(s)" "GAUGE(s)" "COUNT"
        while IFS='|' read -r src mth sc avg g cnt; do
          printf "      %-28s %-6s %-6s %-12s %-12s %s\n" "$src" "$mth" "$sc" "$avg" "$g" "$cnt"
        done <<< "$breakdown"
      fi
    fi
  fi

  unset -f _tc _tf _gauge_stats _hpct _latency_by_label _resolve_ref _label_for_ts _fmt_ref
}

snapshot() {
  if [ "$1" = "-h" -o "$1" = "--help" ]
  then
    echo "Usage: snapshot [-h|--help]; Creates a snapshot directory with process data & configuration."
    echo "                [target dir]; Directory where information is stored ('/tmp/snapshots/last' by default)."
    return 0
  fi

  local dir=${1:-"/tmp/snapshots/$(date +'%y%m%d.%H%M%S')"}
  mkdir -p ${dir} || return 1

  if [ -n "$1" ]
  then
    [ -n "$(ls -A ${dir})" ] && echo "Target directory '${dir}' is not empty !" && return 1
  else
    local last=/tmp/snapshots/last
    rm -f ${last} && ln -s $(basename ${dir}) ${last}
  fi

  mkdir -p ${dir}/{general,schemas,server,client}

  # General
  configuration && pretty > ${dir}/general/configuration.json
  schema && pretty > ${dir}/general/schema.json
  vault && pretty > ${dir}/general/vault.json
  files && pretty > ${dir}/general/files.json
  files_configuration && pretty > ${dir}/general/files-configuration.json
  udp_sockets && pretty > ${dir}/general/udp-sockets.json
  metrics > ${dir}/general/metrics.txt

  # Schemas
  schema_schema && pretty > ${dir}/schemas/schema.json
  vault_schema && pretty > ${dir}/schemas/vault.json
  server_matching_schema && pretty > ${dir}/schemas/server-matching.json
  server_provision_schema && pretty > ${dir}/schemas/server-provision.json
  client_endpoint_schema && pretty > ${dir}/schemas/client-endpoint.json
  client_provision_schema && pretty > ${dir}/schemas/client-provision.json

  # Server
  server_configuration && pretty > ${dir}/server/configuration.json
  server_data_configuration && pretty > ${dir}/server/data-configuration.json
  server_matching && pretty > ${dir}/server/matching.json
  server_provision && pretty > ${dir}/server/provision.json
  server_provision_unused && pretty > ${dir}/server/provision-unused.json
  server_data --summary -1 && pretty > ${dir}/server/data-summary.json
  echo | server_data && pretty > ${dir}/server/data.json
  pushd ${dir}/server ; server_data --dump ; [ -d server-data-sequences ] && mv server-data-sequences data-sequences ; popd

  # Client
  client_data_configuration && pretty > ${dir}/client/data-configuration.json
  client_endpoint && pretty > ${dir}/client/endpoint.json
  client_provision && pretty > ${dir}/client/provision.json
  client_provision_unused && pretty > ${dir}/client/provision-unused.json
  client_data --summary -1 && pretty > ${dir}/client/data-summary.json
  echo | client_data && pretty > ${dir}/client/data.json
  pushd ${dir}/client ; client_data --dump ; [ -d client-data-sequences ] && mv client-data-sequences data-sequences ; popd

  echo
  echo "Created snapshot at:"
  if [ -n "$1" ]
  then
    readlink -f ${dir}
  else
    ls -l ${last}
  fi
}

server_example() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: server_example [-h|--help]; Basic server configuration examples. Try: source <(server_example)" && return 0

  # If h2agent is up, we will suggest as example the same server matching configuration detected, to avoid breaking its behaviour
  #  (provisions and schemas are probably not harmful because of their dummy names):
  local foo_server_matching=$(curl -s --http2-prior-knowledge $(admin_url)/server-matching | jq '.' -c)
  [ -z "${foo_server_matching}" ] && foo_server_matching="{\"algorithm\":\"FullMatching\"}" # fallback to basic example

  local traffic_server_api_path=
  [ -n "${TRAFFIC_SERVER_API}" ] && traffic_server_api_path="/${TRAFFIC_SERVER_API}"

  cat << EOF

  # Configure 'dummySchemaId' to be referenced later:
  cat << ! > /tmp/dummySchema.json
  {
    "\\\$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "additionalProperties": true,
    "properties": {
      "foo": { "type": "number" }
    },
    "required": [ "foo" ]
  }
!
  jq --arg id "dummySchemaId" '. | { id: \$id, schema: . }' /tmp/dummySchema.json > /tmp/h2agent_dummySchema.json
  schema /tmp/h2agent_dummySchema.json # configuration

  ###############
  # SERVER MOCK #
  ###############

  # Configure classification algorithm:
  echo '${foo_server_matching}' > /tmp/dummyServerMatching.json
  server_matching /tmp/dummyServerMatching.json # configuration

  # Configure basic server provision to answer the same request received:
  cat << ! > /tmp/dummyServerProvision.json
  {
    "requestMethod": "POST",
    "requestUri": "${traffic_server_api_path}/my/dummy/path",
    "requestSchemaId": "dummySchemaId",
    "responseSchemaId": "dummySchemaId",
    "responseDelayMs": 0,
    "responseCode": 201,
    "responseHeaders": {
      "content-type": "application/json"
    },
    "transform": [
      { "source": "request.body", "target": "response.body.json.object" }
    ]
  }
!
  server_provision --clean
  server_provision /tmp/dummyServerProvision.json # configuration

  # Check configuration
  schema && server_matching && server_provision

  # Test it !
  server_data --clean
  \${CURL} -d'{"foo":1, "bar":2}' -H'Content-Type: application/json' $(traffic_url)${traffic_server_api_path}/my/dummy/path ; echo # must respond 201
  \${CURL} -d'{"foo":"hi", "bar":2}' -H'Content-Type: application/json' $(traffic_url)${traffic_server_api_path}/my/dummy/path ; echo # must respond 400 (foo value is not numeric)

EOF
}

client_example() {
  [ "$1" = "-h" -o "$1" = "--help" ] && echo "Usage: client_example [-h|--help]; Basic client configuration examples. Try: source <(client_example)" && return 0

  cat << EOF

  # CLIENT EXAMPLE IMPLEMENTATION PENDING
  echo TODO !

EOF
}

help() {
  echo
  echo "===== ${PNAME} operation helpers ====="
  echo "Management interface: https://github.com/testillano/h2agent#management-interface"
  echo "Usage: help; This help summary."
  echo
  echo "=== Internal Functions And Variables ==="
  echo -n "traffic_url: $(traffic_url) (TRAFFIC_PORT=${TRAFFIC_PORT}"
  [ -n "${TRAFFIC_SERVER_API}" ] && echo -n "; TRAFFIC_SERVER_API=${TRAFFIC_SERVER_API}"
  echo ")"
  echo "admin_url:   $(admin_url) (ADMIN_PORT=${ADMIN_PORT})"
  echo "metrics_url: $(metrics_url) (METRICS_PORT=${METRICS_PORT})"
  echo "do_curl:     CURL=\"${CURL}\"; SCHEME=${SCHEME}; SERVER_ADDR=${SERVER_ADDR}"
  export -f traffic_url admin_url metrics_url do_curl
  echo
  echo "=== General Resources' Functions ==="
  for f in schema vault files files_configuration udp_sockets configuration; do ${f} -h | head -n 1; export -f ${f} ; done
  echo
  echo "=== Traffic Server Functions === "
  for f in server_configuration server_data_configuration server_matching server_provision server_provision_unused server_data; do ${f} -h | head -n 1; export -f ${f} ; done
  echo
  echo "=== Traffic Client Functions === "
  for f in client_data_configuration client_endpoint client_provision client_provision_trigger client_provision_cps client_provision_unused client_data traffic_summary; do ${f} -h | head -n 1; export -f ${f} ; done
  echo
  echo "=== Operation Schemas' Functions === "
  for f in schema_schema vault_schema server_matching_schema server_provision_schema client_endpoint_schema client_provision_schema; do ${f} -h | head -n 1; export -f ${f} ; done
  echo
  echo "=== Auxiliary Functions === "
  for f in pretty raw trace metrics snapshot check_storage server_example client_example; do ${f} -h | head -n 1; export -f ${f} ; done
  echo
}

#############
# EXECUTION #
#############

# Check dependencies:
if ! type curl &>/dev/null; then echo "Missing required dependency (curl) !" ; return 1 ; fi
if ! type jq &>/dev/null; then echo "Missing required dependency (jq) !" ; return 1 ; fi

# Initialize temporary and show help
touch ${_H2A_CURL_OUT}
help

