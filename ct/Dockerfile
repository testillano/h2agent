ARG base_tag=latest
FROM alpine:${base_tag}
MAINTAINER testillano

LABEL testillano.ct_h2agent.description="Docker image for h2agent component test"

RUN apk update && apk add \
    python3 \
    py3-pip \
    vim curl jq

COPY src /pytest
WORKDIR /pytest

RUN python3 -m venv /venv && source /venv/bin/activate && pip3 install -r requirements.txt

CMD [ "sleep", "infinity" ]
