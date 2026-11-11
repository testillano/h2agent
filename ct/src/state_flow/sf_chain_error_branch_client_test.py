import pytest
import json
import os
import time
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_CLIENT_ENDPOINT_URI, ADMIN_CLIENT_PROVISION_URI, ADMIN_CLIENT_DATA_URI


H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))

# After a failure, the client chain goes on with the provision of the same id whose
# inState is the exact failure state ('__core:failed:<state>:<reason>'), else the
# one of its stage ('__core:failed:<state>'), else the generic '__core:failed'; with
# none the chain ends. A failure within an error branch ends the chain.

def step(flowId, method, uri, inState = None, outState = None, expected = None, onResponseTransform = None):
  provision = {"id": flowId, "endpoint": "loopback", "requestMethod": method, "requestUri": uri}
  if inState: provision["inState"] = inState
  if outState: provision["outState"] = outState
  if expected: provision["expectedResponseStatusCode"] = expected
  if onResponseTransform: provision["onResponseTransform"] = onResponseTransform
  return provision


def clientEvents(h2ac_admin, method, uri):
  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI + "?clientEndpointId=loopback&requestMethod=" + method + "&requestUri=" + uri)
  if response["status"] == 204: return []
  assert response["status"] == 200
  return response["body"]["events"]


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision_error_branches(h2ac_admin):

  # Server provisions (responders): 'create' always answers 500
  server_provisions = [
    {"requestMethod": "POST", "requestUri": "/app/v1/err-branch/create", "responseCode": 500},
    {"requestMethod": "DELETE", "requestUri": "/app/v1/err-branch/cleanup", "responseCode": 204},
    {"requestMethod": "GET", "requestUri": "/app/v1/err-branch/generic", "responseCode": 200}
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, server_provisions)
  assert response["status"] == 201

  # Loopback client endpoint
  endpoint = {"id": "loopback", "host": H2AGENT_HOST, "port": H2AGENT_TRAFFIC_PORT, "secure": False, "permit": True}
  response = h2ac_admin.postDict(ADMIN_CLIENT_ENDPOINT_URI, endpoint)
  assert response["status"] == 201

  failOnStatus = [{"from": {"source": "responseStatusCode"}, "filter": [{"op": "equalTo", "value": "201", "not": True}],
                   "to": {"target": "fail", "reason": "create-rejected"}}]

  provisions = [
    # exact branch preferred over the generic one:
    step("exactFlow", "POST", "/app/v1/err-branch/create", outState = "next", expected = 201),
    step("exactFlow", "DELETE", "/app/v1/err-branch/cleanup", inState = "__core:failed:initial:expectedResponseStatusCode", expected = 204),
    step("exactFlow", "GET", "/app/v1/err-branch/generic", inState = "__core:failed"),
    # generic branch, reached from a 'fail' target:
    step("genericFlow", "POST", "/app/v1/err-branch/create", outState = "next", onResponseTransform = failOnStatus),
    step("genericFlow", "GET", "/app/v1/err-branch/generic", inState = "__core:failed"),
    # a failure within the branch ends the chain (the generic branch does not catch it again):
    step("nestedFlow", "POST", "/app/v1/err-branch/create", expected = 201),
    step("nestedFlow", "GET", "/app/v1/err-branch/generic", inState = "__core:failed", expected = 201),
    # stage branch (any failure of the step 'next'), preferred over the generic one:
    step("stageFlow", "GET", "/app/v1/err-branch/generic", outState = "next"),
    step("stageFlow", "POST", "/app/v1/err-branch/create", inState = "next", expected = 201),
    step("stageFlow", "DELETE", "/app/v1/err-branch/cleanup", inState = "__core:failed:next", expected = 204),
    step("stageFlow", "GET", "/app/v1/err-branch/generic", inState = "__core:failed"),
    # no branch: the chain ends at the failure
    step("noBranchFlow", "POST", "/app/v1/err-branch/create", outState = "next", expected = 201),
    step("noBranchFlow", "GET", "/app/v1/err-branch/generic", inState = "next")
  ]
  response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, provisions)
  assert response["status"] == 201


def trigger(h2ac_admin, flowId):
  response = h2ac_admin.delete(ADMIN_CLIENT_DATA_URI)
  response = h2ac_admin.post(ADMIN_CLIENT_PROVISION_URI + "/" + flowId + "/trigger")
  assert response["status"] == 200
  time.sleep(1) # async chain


@pytest.mark.client
def test_002_exact_branch_is_preferred(h2ac_admin):

  trigger(h2ac_admin, "exactFlow")

  created = clientEvents(h2ac_admin, "POST", "/app/v1/err-branch/create")
  assert len(created) == 1
  assert created[0]["state"] == "__core:failed:initial:expectedResponseStatusCode"
  assert created[0]["failures"] == [{"reason": "expectedResponseStatusCode", "expected": 201, "got": 500}]

  cleanup = clientEvents(h2ac_admin, "DELETE", "/app/v1/err-branch/cleanup")
  assert len(cleanup) == 1
  assert cleanup[0]["previousState"] == "__core:failed:initial:expectedResponseStatusCode"
  assert cleanup[0]["state"] == "road-closed"
  assert "failures" not in cleanup[0]

  assert clientEvents(h2ac_admin, "GET", "/app/v1/err-branch/generic") == []

  # Sequence entries carry the state of their event and, when it failed, its failures:
  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI + "/sequence")
  assert response["status"] == 200
  entries = response["body"]
  assert [(e["direction"], e["method"]) for e in entries] == [("send", "POST"), ("recv", "POST"), ("send", "DELETE"), ("recv", "DELETE")]
  for e in entries[:2]:
    assert e["state"] == "__core:failed:initial:expectedResponseStatusCode"
    assert e["failures"] == [{"reason": "expectedResponseStatusCode", "expected": 201, "got": 500}]
  for e in entries[2:]:
    assert e["state"] == "road-closed"
    assert "failures" not in e


@pytest.mark.client
def test_003_generic_branch_catches_any_failure(h2ac_admin):

  trigger(h2ac_admin, "genericFlow")

  created = clientEvents(h2ac_admin, "POST", "/app/v1/err-branch/create")
  assert len(created) == 1
  assert created[0]["state"] == "__core:failed:initial:create-rejected"
  assert created[0]["failures"] == [{"reason": "create-rejected", "detail": 500}]

  generic = clientEvents(h2ac_admin, "GET", "/app/v1/err-branch/generic")
  assert len(generic) == 1
  assert generic[0]["previousState"] == "__core:failed"


@pytest.mark.client
def test_004_failure_within_branch_ends_chain(h2ac_admin):

  trigger(h2ac_admin, "nestedFlow")

  generic = clientEvents(h2ac_admin, "GET", "/app/v1/err-branch/generic")
  assert len(generic) == 1 # sent once: no loop over the generic branch
  assert generic[0]["state"] == "__core:failed:__core_failed:expectedResponseStatusCode" # stage: the branch inState, ':' as '_'


@pytest.mark.client
def test_005_no_branch_ends_chain(h2ac_admin):

  trigger(h2ac_admin, "noBranchFlow")

  created = clientEvents(h2ac_admin, "POST", "/app/v1/err-branch/create")
  assert len(created) == 1
  assert created[0]["state"] == "__core:failed:initial:expectedResponseStatusCode"
  assert clientEvents(h2ac_admin, "GET", "/app/v1/err-branch/generic") == []


@pytest.mark.client
def test_006_stage_branch_catches_any_failure_of_its_step(h2ac_admin):

  trigger(h2ac_admin, "stageFlow")

  created = clientEvents(h2ac_admin, "POST", "/app/v1/err-branch/create")
  assert len(created) == 1
  assert created[0]["previousState"] == "next"
  assert created[0]["state"] == "__core:failed:next:expectedResponseStatusCode"

  cleanup = clientEvents(h2ac_admin, "DELETE", "/app/v1/err-branch/cleanup")
  assert len(cleanup) == 1
  assert cleanup[0]["previousState"] == "__core:failed:next"

  generic = clientEvents(h2ac_admin, "GET", "/app/v1/err-branch/generic")
  assert len(generic) == 1 # only the first step (the generic branch is not reached)
  assert generic[0]["state"] == "next"
