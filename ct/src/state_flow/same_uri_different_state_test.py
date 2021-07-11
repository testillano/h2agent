import pytest
import json
from conftest import VALID_PROVISIONS__RESPONSE_BODY


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_provision_two_uris_with_different_states_on_admin_interface(admin_provision):

  # Provision
  admin_provision("provision.state_flow.same_uri_different_state_test.json", responseBodyRef=VALID_PROVISIONS__RESPONSE_BODY)


@pytest.mark.server
def test_002_i_want_to_send_get_requests_for_provisioned_data_on_traffic_interface(h2ac_traffic):

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

