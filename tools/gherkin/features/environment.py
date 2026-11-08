"""
Behave environment hooks for the h2agent Gherkin driver.

Configuration via environment variables:
  H2AGENT_ADMIN_ENDPOINT   - admin API base (default: http://localhost:8074)
  H2AGENT_TRAFFIC_ENDPOINT - traffic base (default: http://localhost:8000)

User data (behave -D <name>=<value>):
  dump=<output_dir>  record the API calls as numbered json files (no h2agent)
  timeout=<seconds>  how long data/vault assertions wait for the expected
                     value (default 3: traffic and client flows are async)
"""

import os
import sys

from h2agent_lib import DumpRecorder, H2AgentClient, flush_provision


def before_all(context):
    dump_dir = context.config.userdata.get("dump")
    context.dump = DumpRecorder(dump_dir) if dump_dir else None
    context.timeout = float(context.config.userdata.get("timeout", 3))

    if context.dump:
        context.h2 = None  # no connection needed
    else:
        admin = os.environ.get("H2AGENT_ADMIN_ENDPOINT", "http://localhost:8074")
        traffic = os.environ.get("H2AGENT_TRAFFIC_ENDPOINT", "http://localhost:8000")
        # Every scenario wipes the agent first: say which one it is
        print(f"h2agent under test: admin {admin}, traffic {traffic}", file=sys.stderr)
        context.h2 = H2AgentClient(admin, traffic)


def before_scenario(context, scenario):
    if context.h2 and "no_clean" not in scenario.tags:
        context.h2.clean_all()
    context.pending = None        # provision being described (kind, body)
    context.last_response = None  # last traffic response ("I send ...")


def after_scenario(context, scenario):
    # A provision described by the last steps has no later step to send it
    if scenario.status != "failed":
        flush_provision(context)


def after_all(context):
    if context.h2:
        context.h2.session.close()
