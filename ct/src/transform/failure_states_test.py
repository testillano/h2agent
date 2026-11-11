import pytest
import json
import os
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI, ADMIN_CLIENT_ENDPOINT_URI, ADMIN_CLIENT_PROVISION_URI, ADMIN_CLIENT_DATA_URI


H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))

# Failure states end to end (proposal examples I and K):
#   I: a server answers 400 with the validation report and marks the failure.
#   K: the report of a client failure is read from another provision (clientEvent source).

WRONG_SUPI_REPORT = "JsonConstraint FAILED: expected value for key 'supi' differs regarding validated source"


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision(h2ac_admin):

  server_provisions = [
    # I: answer 400 with the report (kept once in a var) and mark the failure; a valid body is answered 200
    {"requestMethod": "POST", "requestUri": "/app/v1/fs/supi-check", "responseCode": 200,
     "transform": [
       {"from": {"source": "requestBody"},
        "filter": [{"op": "jsonConstraint", "value": {"supi": "imsi-001019000040000"}, "not": True}],
        "to": {"target": "var", "name": "badRequest"}},
       {"from": {"source": "var", "name": "badRequest"}, "to": {"target": "responseBodyText"}},
       {"from": {"source": "literal", "value": "400"},
        "filter": [{"op": "conditionVar", "name": "badRequest"}], "to": {"target": "responseStatusCode"}},
       {"from": {"source": "var", "name": "badRequest"}, "to": {"target": "fail", "reason": "wrong-supi"}}
     ]},
    # K: responder which makes the client step fail, and a reader of the failure detail
    {"requestMethod": "POST", "requestUri": "/app/v1/fs/create", "responseCode": 500},
    {"requestMethod": "GET", "requestUri": "/app/v1/fs/report", "responseCode": 200,
     "transform": [
       {"from": {"source": "clientEvent", "clientEndpointId": "loopback", "requestMethod": "POST",
                 "requestUri": "/app/v1/fs/create", "eventNumber": -1, "eventPath": "/failures/0/detail"},
        "to": {"target": "responseBodyText"}}
     ]}
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, server_provisions)
  assert response["status"] == 201

  endpoint = {"id": "loopback", "host": H2AGENT_HOST, "port": H2AGENT_TRAFFIC_PORT, "secure": False, "permit": True}
  response = h2ac_admin.postDict(ADMIN_CLIENT_ENDPOINT_URI, endpoint)
  assert response["status"] == 201

  client_provision = {"id": "fsCreate", "endpoint": "loopback", "requestMethod": "POST", "requestUri": "/app/v1/fs/create",
    "onResponseTransform": [
      {"from": {"source": "responseStatusCode"}, "filter": [{"op": "equalTo", "value": "201", "not": True}],
       "to": {"target": "fail", "reason": "create-rejected"}}
    ]}
  response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, client_provision)
  assert response["status"] == 201


@pytest.mark.server
def test_002_server_answers_the_report_and_marks_the_failure(h2ac_admin, h2ac_traffic):

  # Valid body: the validation rules are skipped (the report only arrives on failure)
  response = h2ac_traffic.postDict("/app/v1/fs/supi-check", {"supi": "imsi-001019000040000"})
  assert response["status"] == 200

  # Invalid body: 400 with the report, and the key is in the failure state
  response = h2ac_traffic.postDict("/app/v1/fs/supi-check", {"supi": "imsi-999"})
  assert response["status"] == 400
  assert response["body"] == WRONG_SUPI_REPORT

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/fs/supi-check")
  assert response["status"] == 200
  events = response["body"]["events"]
  assert len(events) == 2
  assert events[0]["state"] == "initial"
  assert "failures" not in events[0]
  assert events[1]["state"] == "__core:failed:initial:wrong-supi"
  assert events[1]["failures"] == [{"reason": "wrong-supi", "detail": WRONG_SUPI_REPORT}]


@pytest.mark.client
def test_003_failure_detail_read_from_another_provision(h2ac_admin, h2ac_traffic):

  response = h2ac_admin.post(ADMIN_CLIENT_PROVISION_URI + "/fsCreate/trigger")
  assert response["status"] == 200

  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI + "/wait?clientProvisionId=fsCreate&statePrefix=__core:failed:&timeoutMs=5000")
  assert response["status"] == 200
  assert response["body"]["state"] == "__core:failed:initial:create-rejected"

  # The detail (the status code which reached the 'fail' target) through the clientEvent source:
  response = h2ac_traffic.get("/app/v1/fs/report")
  assert response["status"] == 200
  assert response["body"] == 500
