import pytest
import json
import os
import time
from conftest import (
    ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI, ADMIN_CLIENT_PROVISION_URI,
    VALID_CLIENT_ENDPOINT__RESPONSE_BODY, string2dict, CLIENT_ENDPOINT_TEMPLATE
)

# The recipes of the API guide (docs/api/README.md#recipes), as written there.

H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))
CALLBACKS_ENDPOINT = string2dict(CLIENT_ENDPOINT_TEMPLATE, id="callbacks", port=H2AGENT_TRAFFIC_PORT)
CALLBACKS_ENDPOINT["host"] = H2AGENT_HOST
JSON = {"content-type": "application/json"}


@pytest.mark.transform
def test_001_numeric_comparison(admin_cleanup, h2ac_admin, h2ac_traffic):
  admin_cleanup()
  provision = {"requestMethod": "POST", "requestUri": "/app/v1/recipes/members", "responseCode": 201,
    "transform": [
      {"from": {"source": "requestBody", "path": "/age"}, "to": {"target": "var", "name": "age"}},
      {"from": {"source": "math", "expression": "@{age} >= 18 and @{age} < 65"}, "to": {"target": "var", "name": "adult"}},
      {"from": {"source": "literal", "value": "403"}, "filter": [{"op": "conditionVar", "name": "adult", "not": True}], "to": {"target": "responseStatusCode"}}
    ]}
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provision)["status"] == 201
  for age, code in ((17, 403), (18, 201), (64.5, 201), (65, 403)):
    assert h2ac_traffic.postDict("/app/v1/recipes/members", {"age": age}, JSON)["status"] == code, age


@pytest.mark.transform
def test_002_fixed_length(admin_cleanup, h2ac_admin, h2ac_traffic):
  admin_cleanup()
  provision = {"requestMethod": "GET", "requestUri": "/app/v1/recipes/ticket", "responseCode": 200, "responseBody": {},
    "responseHeaders": JSON,
    "transform": [
      {"from": {"source": "recvseq"}, "to": {"target": "var", "name": "recvseq"}},
      {"from": {"source": "recvseq"}, "filter": [{"op": "sum", "value": 1000000}, {"op": "regexReplace", "rgx": "^1", "fmt": ""}], "to": {"target": "var", "name": "ticket"}},
      {"from": {"source": "var", "name": "ticket"}, "to": {"target": "responseBody", "path": "/ticket"}},
      {"from": {"source": "literal", "value": "report-2026-final"}, "filter": [{"op": "regexReplace", "rgx": "^.*(.{4})$", "fmt": "$1"}], "to": {"target": "responseBody", "path": "/tail"}},
      {"from": {"source": "var", "name": "recvseq"}, "to": {"target": "responseBody", "path": "/recvseq"}}
    ]}
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provision)["status"] == 201
  body = h2ac_traffic.get("/app/v1/recipes/ticket")["body"]
  assert body["ticket"] == "{:06d}".format(body["recvseq"])
  assert body["tail"] == "inal"


@pytest.mark.transform
def test_003_branch_by_content(admin_cleanup, h2ac_admin, h2ac_traffic):
  admin_cleanup()
  provision = {
    "requestMethod": "POST", "requestUri": "/app/v1/orders", "responseCode": 201, "responseBody": {"result": "created"},
    "responseHeaders": JSON,
    "transform": [
      {"from": {"source": "requestBody", "path": "/action"}, "filter": [{"op": "equalTo", "value": "cancel"}], "to": {"target": "var", "name": "cancel"},
       "onFilterFail": [{"from": {"source": "literal", "value": ""}, "to": {"target": "var", "name": "cancel"}}]},
      {"from": {"source": "literal", "value": "200"}, "filter": [{"op": "conditionVar", "name": "cancel"}], "to": {"target": "responseStatusCode"}},
      {"from": {"source": "literal", "value": "cancelled"}, "filter": [{"op": "conditionVar", "name": "cancel"}], "to": {"target": "responseBody", "path": "/result"}},
      {"from": {"source": "literal", "value": "cancelled"}, "filter": [{"op": "conditionVar", "name": "cancel"}], "to": {"target": "outState"}}
    ]}
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provision)["status"] == 201

  response = h2ac_traffic.postDict("/app/v1/orders", {"action": "create"}, JSON)
  assert response["status"] == 201 and response["body"] == {"result": "created"}
  response = h2ac_traffic.postDict("/app/v1/orders", {"action": "create"}, JSON)
  assert response["status"] == 201
  response = h2ac_traffic.postDict("/app/v1/orders", {"action": "cancel"}, JSON)
  assert response["status"] == 200 and response["body"] == {"result": "cancelled"}

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/orders&eventNumber=-1")
  assert response["body"]["state"] == "cancelled"


@pytest.mark.transform
def test_004_deferred_notification(admin_cleanup, h2ac_admin, h2ac_traffic, admin_client_endpoint):
  admin_cleanup()
  admin_client_endpoint(CALLBACKS_ENDPOINT, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)
  servers = [
    {"requestMethod": "POST", "requestUri": "/app/v1/jobs", "responseCode": 202,
     "transform": [{"from": {"source": "literal", "value": "1"}, "to": {"target": "clientProvision", "id": "notifyDone", "inState": "initial"}}]},
    {"requestMethod": "POST", "requestUri": "/app/v1/events", "responseCode": 200}
  ]
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, servers)["status"] == 201
  client = {"id": "notifyDone", "endpoint": "callbacks", "requestMethod": "POST", "requestUri": "/app/v1/events", "requestDelayMs": 1000,
            "requestHeaders": JSON,
            "transform": [{"from": {"source": "serverEvent", "requestMethod": "POST", "requestUri": "/app/v1/jobs", "eventNumber": -1, "eventPath": "/requestBody/id"},
                           "to": {"target": "requestBody", "path": "/job"}}]}
  assert h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, client)["status"] == 201

  start = time.time()
  assert h2ac_traffic.postDict("/app/v1/jobs", {"id": "job-7"}, JSON)["status"] == 202
  assert time.time() - start < 0.8  # the response is not delayed

  def notification():
    response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/events&eventNumber=1")
    return response["body"] if response["status"] == 200 else None

  deadline = time.time() + 5
  while time.time() < deadline and not notification(): time.sleep(0.1)
  event = notification()
  assert event and event["requestBody"] == {"job": "job-7"}
  job = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/jobs&eventNumber=1")["body"]
  assert event["receptionTimestampUs"] - job["receptionTimestampUs"] >= 1000000  # notified after the delay
