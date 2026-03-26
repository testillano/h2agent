"""
Behave environment hooks for h2agent Gherkin driver.

Configuration via environment variables:
  H2AGENT_ADMIN_ENDPOINT  - admin API base (default: http://localhost:8074)
  H2AGENT_TRAFFIC_ENDPOINT - traffic base (default: http://localhost:8000)

Dump mode (no h2agent needed):
  behave -D dump=<output_dir> features/my.feature
"""

import os
import sys
import json
import requests


class H2AgentClient:
    """Thin wrapper around h2agent admin and traffic REST API."""

    def __init__(self, admin_endpoint, traffic_endpoint):
        self.admin = admin_endpoint.rstrip("/")
        self.traffic = traffic_endpoint.rstrip("/")
        self.session = requests.Session()

    def admin_url(self, path):
        return f"{self.admin}/admin/v1/{path.lstrip('/')}"

    def traffic_url(self, path):
        return f"{self.traffic}/{path.lstrip('/')}"

    def post_json(self, url, body):
        return self.session.post(url, json=body)

    def put_json(self, url, body):
        return self.session.put(url, json=body)

    def get(self, url, **kwargs):
        return self.session.get(url, **kwargs)

    def delete(self, url, **kwargs):
        return self.session.delete(url, **kwargs)

    def put(self, url, **kwargs):
        return self.session.put(url, **kwargs)

    def clean_all(self):
        for resource in ("server-provision", "server-data",
                         "client-provision", "client-endpoint", "client-data",
                         "schema", "vault"):
            self.delete(self.admin_url(resource))


class DumpRecorder:
    """Records API calls as numbered JSON files instead of executing them."""

    def __init__(self, output_dir):
        if os.path.exists(output_dir):
            print(f"ERROR: dump directory already exists: {output_dir}", file=sys.stderr)
            sys.exit(1)
        self.output_dir = output_dir
        self.seq = 0
        os.makedirs(output_dir)

    def record(self, label, method, path, body=None, params=None, is_traffic=False):
        self.seq += 1
        safe_label = label.replace("/", "_").replace(" ", "-")
        filename = f"{self.seq:02d}.{safe_label}.json"

        entry = {"method": method}
        if is_traffic:
            entry["path"] = path
            entry["type"] = "traffic"
        else:
            entry["path"] = f"/admin/v1/{path}"
        if params:
            entry["queryParams"] = params
        if body is not None:
            entry["body"] = body

        filepath = os.path.join(self.output_dir, filename)
        with open(filepath, "w") as f:
            json.dump(entry, f, indent=2, ensure_ascii=False)
            f.write("\n")

        return filepath


def before_all(context):
    dump_dir = context.config.userdata.get("dump")
    context.dump = DumpRecorder(dump_dir) if dump_dir else None

    if context.dump:
        context.h2 = None  # no connection needed
    else:
        admin = os.environ.get("H2AGENT_ADMIN_ENDPOINT", "http://localhost:8074")
        traffic = os.environ.get("H2AGENT_TRAFFIC_ENDPOINT", "http://localhost:8000")
        context.h2 = H2AgentClient(admin, traffic)

    context.provision = {}
    context.last_response = None


def before_scenario(context, scenario):
    if context.h2 and "no_clean" not in scenario.tags:
        context.h2.clean_all()
    context.provision = {}
    context.last_response = None
