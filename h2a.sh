#!/bin/bash
# Run './build.sh --auto' to have docker image available:
docker run --rm -it --network=host ghcr.io/testillano/h2agent:latest $@ # accepts additional arguments, i.e: -l Debug
