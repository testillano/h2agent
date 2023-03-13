#!/bin/bash
# Optionally accepts additional arguments for h2agent executable: $@
docker run --rm -it --network=host ghcr.io/testillano/h2agent:latest $@
