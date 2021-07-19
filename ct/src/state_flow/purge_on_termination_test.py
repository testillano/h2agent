import pytest
import json
from conftest import VALID_PROVISIONS__RESPONSE_BODY, ADMIN_DATA_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_provision_two_uris_with_different_states_and_final_purge_on_admin_interface(admin_provision):

  # Provision
  admin_provision("state_flow/purge_on_termination_test/provision.json", responseBodyRef=VALID_PROVISIONS__RESPONSE_BODY)


@pytest.mark.server
def test_002_i_want_to_send_get_requests_for_provisioned_data_and_check_final_purge_on_traffic_interface(h2ac_traffic, h2ac_admin):

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1-state-initial" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check that server data exists
  response = h2ac_admin.get(ADMIN_DATA_URI)
  assert response["status"] == 200

  # Send GET again:
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1-state-final" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check that THERE IS NO server data
  response = h2ac_admin.get(ADMIN_DATA_URI)
  assert response["status"] == 204

  # Send GET again to confirm rotation:
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1-state-initial" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check that server data exists again
  response = h2ac_admin.get(ADMIN_DATA_URI)
  assert response["status"] == 200

