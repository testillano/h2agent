import pytest
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI


# A request body announced as json (application/json, application/<x>+json) which does
# not parse: answered with 400, transformations skipped, event failed with 'jsonParse',
# and the received text kept as the body.

URI = "/app/v1/json-parse/item"


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision(h2ac_admin):

  provision = {"requestMethod": "POST", "requestUri": URI, "responseCode": 201, "outState": "created",
               "transform": [{"from": {"source": "requestBody", "path": "/name"}, "to": {"target": "responseBodyText"}}]}
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provision)
  assert response["status"] == 201


@pytest.mark.server
def test_002_invalid_json_body_fails(h2ac_admin, h2ac_traffic):

  response = h2ac_traffic.post(URI, '{"name":', {"content-type": "application/problem+json"})
  assert response["status"] == 400

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=" + URI + "&eventNumber=-1")
  assert response["status"] == 200
  event = response["body"]
  assert event["state"] == "__core:failed:initial:jsonParse"
  assert event["requestBody"] == '{"name":'
  assert len(event["failures"]) == 1
  assert event["failures"][0]["reason"] == "jsonParse"
  assert "parse error" in event["failures"][0]["error"]


@pytest.mark.server
def test_003_valid_json_body_with_parameters(h2ac_admin, h2ac_traffic):

  # the key failed: a new key for the valid case
  response = h2ac_admin.delete(ADMIN_SERVER_DATA_URI)
  response = h2ac_traffic.post(URI, '{"name":"bob"}', {"content-type": "application/json; charset=utf-8"})
  assert response["status"] == 201
  assert response["body"] == "bob"
