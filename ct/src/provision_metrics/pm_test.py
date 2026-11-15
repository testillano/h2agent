import pytest
import os
import re
import time
import urllib.request
from conftest import (
    ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI, ADMIN_CLIENT_PROVISION_URI,
    VALID_CLIENT_ENDPOINT__RESPONSE_BODY, string2dict, CLIENT_ENDPOINT_TEMPLATE
)

# Per-provision metrics: series per provision id and stage (inState), scraped from the
# prometheus port. Server provisions are measured only when they have an 'id'.

H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))
METRICS_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP_METRICS', 8080))
LOOPBACK_ENDPOINT = string2dict(CLIENT_ENDPOINT_TEMPLATE, id="loopback", port=H2AGENT_TRAFFIC_PORT)
LOOPBACK_ENDPOINT["host"] = H2AGENT_HOST


def scrape():
  with urllib.request.urlopen("http://{}:{}/metrics".format(H2AGENT_HOST, METRICS_PORT), timeout=5) as r:
    return r.read().decode()


def value(text, name, **labels):
  """Sum of the samples of 'name' whose labels include the given ones (0 when none)."""
  total = 0.0
  for line in text.splitlines():
    m = re.match(r'^' + re.escape(name) + r'\{([^}]*)\} (\S+)$', line)
    if not m: continue
    found = dict(re.findall(r'(\w+)="([^"]*)"', m.group(1)))
    if all(found.get(k) == str(v) for k, v in labels.items()): total += float(m.group(2))
  return total


@pytest.mark.server
def test_001_server_with_and_without_id(admin_cleanup, h2ac_admin, h2ac_traffic):
  admin_cleanup()
  provisions = [
    {"id": "ct-books", "requestMethod": "GET", "requestUri": "/app/v1/pm/books", "responseCode": 200},
    {"id": "ct-books", "requestMethod": "GET", "requestUri": "/app/v1/pm/books/1", "responseCode": 404},  # same id: aggregated
    {"requestMethod": "GET", "requestUri": "/app/v1/pm/films", "responseCode": 200}                     # no id: not measured
  ]
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provisions)["status"] == 201

  before = scrape()
  for uri, code in (("/app/v1/pm/books", 200), ("/app/v1/pm/books", 200), ("/app/v1/pm/books/1", 404), ("/app/v1/pm/films", 200)):
    assert h2ac_traffic.get(uri)["status"] == code
  time.sleep(0.2)  # responses are counted when sent
  after = scrape()

  name = "h2agent_traffic_server_provision_"
  delta = lambda metric, **l: value(after, name + metric, **l) - value(before, name + metric, **l)
  assert delta("requests_counter", provision="ct-books", state="initial") == 3
  assert delta("responses_counter", provision="ct-books", state="initial", status_code=200) == 2
  assert delta("responses_counter", provision="ct-books", state="initial", status_code=404) == 1
  assert delta("response_delay_seconds_count", provision="ct-books", state="initial") == 3
  assert delta("requests_counter") == 3  # the provision without id adds nothing

  # the id in the event and in the sequence
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/pm/books&eventNumber=1")
  assert response["body"]["serverProvisionId"] == "ct-books"
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/pm/films&eventNumber=1")
  assert "serverProvisionId" not in response["body"]
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/sequence?uriRegex=/app/v1/pm/books$")
  assert response["status"] == 200
  assert all(e["serverProvisionId"] == "ct-books" for e in response["body"])


@pytest.mark.client
def test_002_client_chain_stages(admin_cleanup, h2ac_admin, admin_client_endpoint):
  admin_cleanup()
  admin_client_endpoint(LOOPBACK_ENDPOINT, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)
  servers = [
    {"requestMethod": "POST", "requestUri": "/app/v1/pm/orders", "responseCode": 201},
    {"requestMethod": "PUT", "requestUri": "/app/v1/pm/orders/pay", "responseCode": 200}
  ]
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, servers)["status"] == 201
  steps = [
    {"id": "ctOrder", "endpoint": "loopback", "requestMethod": "POST", "requestUri": "/app/v1/pm/orders", "outState": "pay"},
    {"id": "ctOrder", "inState": "pay", "endpoint": "loopback", "requestMethod": "PUT", "requestUri": "/app/v1/pm/orders/pay"}
  ]
  for step in steps: assert h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, step)["status"] == 201

  before = scrape()
  assert h2ac_admin.post(ADMIN_CLIENT_PROVISION_URI + "/ctOrder/trigger?sequenceBegin=1&sequenceEnd=3&cps=50")["status"] == 202

  name = "h2agent_traffic_client_provision_"
  deadline = time.time() + 5
  while time.time() < deadline:
    after = scrape()
    if value(after, name + "responses_counter", provision="ctOrder", state="pay") - value(before, name + "responses_counter", provision="ctOrder", state="pay") >= 3: break
    time.sleep(0.1)

  delta = lambda metric, **l: value(after, name + metric, **l) - value(before, name + metric, **l)
  assert delta("requests_counter", provision="ctOrder", state="initial") == 3
  assert delta("requests_counter", provision="ctOrder", state="pay") == 3
  assert delta("responses_counter", provision="ctOrder", state="initial", status_code=201) == 3
  assert delta("responses_counter", provision="ctOrder", state="pay", status_code=200) == 3
  assert delta("response_delay_seconds_count", provision="ctOrder", state="initial") == 3
  assert delta("response_delay_seconds_count", provision="ctOrder", state="pay") == 3
