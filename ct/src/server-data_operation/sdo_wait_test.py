import pytest
import json
import os
import threading
import time
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI, ADMIN_CLIENT_ENDPOINT_URI, ADMIN_CLIENT_PROVISION_URI, ADMIN_CLIENT_DATA_URI


H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))

# Data waits: block until an event of a key meets a state condition.

def later(seconds, action):
  thread = threading.Thread(target = lambda: (time.sleep(seconds), action()))
  thread.start()
  return thread


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision(h2ac_admin):

  provisions = [
    {"requestMethod": "POST", "requestUri": "/app/v1/wait/1", "responseCode": 201, "outState": "reauth"},
    {"requestMethod": "POST", "requestUri": "/app/v1/wait/1", "inState": "reauth", "responseCode": 200, "outState": "done"},
    {"requestMethod": "POST", "requestUri": "/app/v1/wait/trap", "responseCode": 204,
     "transform": [{"from": {"source": "literal", "value": "sent"}, "to": {"target": "fail", "reason": "trap-hit"}}]}
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provisions)
  assert response["status"] == 201


@pytest.mark.server
def test_002_server_wait_timeout(h2ac_admin):

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=POST&requestUri=/app/v1/wait/1&state=reauth&timeoutMs=200")
  assert response["status"] == 408
  assert response["body"]["result"] == False
  assert response["body"]["key"] == {"requestMethod": "POST", "requestUri": "/app/v1/wait/1"}


@pytest.mark.server
def test_003_server_wait_met_later_and_already_met(h2ac_admin, h2ac_traffic):

  thread = later(0.3, lambda: h2ac_traffic.postDict("/app/v1/wait/1"))
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=POST&requestUri=/app/v1/wait/1&state=reauth&timeoutMs=5000")
  thread.join()
  assert response["status"] == 200
  body = response["body"]
  assert body["result"] == True
  assert body["key"] == {"requestMethod": "POST", "requestUri": "/app/v1/wait/1"}
  assert body["state"] == "reauth"
  assert body["previousState"] == "initial"
  assert body["eventNumber"] == 1
  assert body["event"]["responseStatusCode"] == 201

  # Already met: answers at once
  start = time.time()
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=POST&requestUri=/app/v1/wait/1&state=reauth&timeoutMs=5000")
  assert response["status"] == 200
  assert time.time() - start < 2


@pytest.mark.server
def test_004_server_wait_next_new_event_ignores_stored(h2ac_admin, h2ac_traffic):

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=POST&requestUri=/app/v1/wait/1&timeoutMs=200")
  assert response["status"] == 408 # the stored event does not count

  thread = later(0.3, lambda: h2ac_traffic.postDict("/app/v1/wait/1"))
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=POST&requestUri=/app/v1/wait/1&timeoutMs=5000")
  thread.join()
  assert response["status"] == 200
  assert response["body"]["state"] == "done"
  assert "eventNumber" not in response["body"]


@pytest.mark.server
def test_005_server_wait_failure_by_prefix_and_uri_regex(h2ac_admin, h2ac_traffic):

  thread = later(0.3, lambda: h2ac_traffic.postDict("/app/v1/wait/trap"))
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=POST&uriRegex=/wait/tr&statePrefix=__core:failed:&timeoutMs=5000")
  thread.join()
  assert response["status"] == 200
  body = response["body"]
  assert body["key"] == {"requestMethod": "POST", "requestUri": "/app/v1/wait/trap"}
  assert body["state"] == "__core:failed:initial:trap-hit"
  assert body["event"]["failures"] == [{"reason": "trap-hit", "detail": "sent"}]


@pytest.mark.admin
def test_006_server_wait_bad_parameters(h2ac_admin):

  for query in ["requestMethod=POST", # no uri
                "requestUri=/app/v1/wait/1", # no method
                "requestMethod=POST&requestUri=/a&uriRegex=a", # both uri forms
                "requestMethod=POST&uriRegex=(", # invalid regex
                "requestMethod=POST&requestUri=/a&state=a&statePrefix=b", # exclusive
                "requestMethod=POST&requestUri=/a&eventNumber=0",
                "requestMethod=POST&requestUri=/a&timeoutMs=-1"]:
    response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?" + query)
    assert response["status"] == 400, query


@pytest.mark.client
def test_007_client_wait(h2ac_admin):

  endpoint = {"id": "loopback", "host": H2AGENT_HOST, "port": H2AGENT_TRAFFIC_PORT, "secure": False, "permit": True}
  response = h2ac_admin.postDict(ADMIN_CLIENT_ENDPOINT_URI, endpoint)
  assert response["status"] == 201
  provision = {"id": "waitFlow", "endpoint": "loopback", "requestMethod": "POST", "requestUri": "/app/v1/wait/1", "expectedResponseStatusCode": 204}
  response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, provision)
  assert response["status"] == 201

  response = h2ac_admin.post(ADMIN_CLIENT_PROVISION_URI + "/waitFlow/trigger")
  assert response["status"] == 200

  # The step fails (the server key is in a state with no provision: 501): the failure is waited by prefix
  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI + "/wait?clientProvisionId=waitFlow&statePrefix=__core:failed:&timeoutMs=5000")
  assert response["status"] == 200
  body = response["body"]
  assert body["key"]["clientProvisionId"] == "waitFlow"
  assert body["key"]["clientEndpointId"] == "loopback"
  assert body["state"] == "__core:failed:initial:expectedResponseStatusCode"

  # Unknown provision: timeout; missing id: 400
  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI + "/wait?clientProvisionId=other&state=x&timeoutMs=200")
  assert response["status"] == 408
  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI + "/wait?state=x")
  assert response["status"] == 400
