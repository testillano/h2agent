ARG base_tag=24.04
FROM ubuntu:${base_tag}
LABEL maintainer="testillano"

LABEL testillano.ct_h2agent.description="Docker image for h2agent component test"

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-venv \
    vim curl jq \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

COPY src /pytest
WORKDIR /pytest

RUN python3 -m venv /venv && . /venv/bin/activate && pip3 install -r requirements.txt

CMD [ "sleep", "infinity" ]
