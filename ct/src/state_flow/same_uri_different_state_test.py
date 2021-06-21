import pytest
import json


@pytest.mark.admin
def test_001_cleanup_provisions(resources, h2ac_admin):
  response = h2ac_admin.delete("/provision/v1/server-provisions")


@pytest.mark.admin
def test_002_i_want_to_provision_two_uris_with_different_states_on_admin_interface(resources, h2ac_admin):

  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }

  # Send POST
  requestBody = resources("server-provision_STATE-initial.json")
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  requestBody = resources("server-provision_STATE-another.json")
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

@pytest.mark.server
def test_003_i_want_to_send_get_requests_for_provisioned_data_on_traffic_interface(resources, h2ac_admin, h2ac_traffic):

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1-state-initial" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Send GET again:
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1-state-another" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Send GET again to confirm rotation:
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1-state-initial" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

