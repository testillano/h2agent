import pytest
from conftest import ADMIN_SERVER_DATA_URI, ADMIN_SERVER_PROVISION_URI


TRACED_PROVISION = {
  "requestMethod": "GET",
  "requestUri": "/app/v1/trace/1",
  "responseCode": 200,
  "transform": [
    {"from": {"source": "literal", "value": "a"}, "to": {"target": "var", "name": "x"}},
    {"from": {"source": "literal", "value": "b"}, "filter": [{"op": "equalTo", "value": "c"}], "to": {"target": "responseBodyText"},
     "onFilterFail": [{"from": {"source": "literal", "value": "fallback"}, "to": {"target": "responseBodyText"}}]}
  ]
}


@pytest.mark.admin
def test_001_i_want_to_store_the_execution_trace_in_server_events(admin_cleanup, h2ac_admin, h2ac_traffic):

  admin_cleanup()
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, TRACED_PROVISION)
  assert response["status"] == 201

  # Disabled by default: no trace stored
  response = h2ac_traffic.get("/app/v1/trace/1")
  assert response["status"] == 200
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/trace/1&eventNumber=-1")
  assert "executionTrace" not in response["body"]

  # Enabled:
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?executionTrace=true")
  assert response["status"] == 200
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/configuration")
  assert response["body"]["executionTrace"] == True

  response = h2ac_traffic.get("/app/v1/trace/1")
  assert response["body"] == "fallback"
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/trace/1&eventNumber=-1")
  assert response["body"]["executionTrace"] == [
    {"rule": "/transform/0", "applied": True},
    {"rule": "/transform/1", "applied": False, "note": "EqualTo is false; rule skipped"},
    {"rule": "/transform/1/onFilterFail/0", "applied": True}
  ]

  # Restore default:
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?executionTrace=false")
  assert response["status"] == 200


@pytest.mark.admin
def test_002_i_want_to_reject_an_invalid_execution_trace_value(h2ac_admin):

  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?executionTrace=yes")
  assert response["status"] == 400
