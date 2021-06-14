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

RUN pip3 install -r requirements.txt

CMD [ "sleep", "infinity" ]
