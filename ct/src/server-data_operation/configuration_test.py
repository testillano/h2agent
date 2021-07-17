import pytest
import json
from conftest import BASIC_FOO_BAR_PROVISION_TEMPLATE, string2dict, ADMIN_DATA_URI


@pytest.mark.admin
def test_001_i_want_to_set_discard_true_and_requests_history_discard_true(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_DATA_URI + "/configuration?discard=true&discardRequestsHistory=true")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": "false", "storeEventsRequestsHistory": "false" }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_002_i_want_to_set_discard_true_and_requests_history_discard_false(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_DATA_URI + "/configuration?discard=true&discardRequestsHistory=false")
  assert response["status"] == 400

  # Check configuration (must be the configuration from previous test, as this one failed):
  response = h2ac_admin.get(ADMIN_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": "false", "storeEventsRequestsHistory": "false" }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_003_i_want_to_set_discard_false_and_requests_history_discard_true(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_DATA_URI + "/configuration?discard=false&discardRequestsHistory=true")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": "true", "storeEventsRequestsHistory": "false" }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_004_i_want_to_set_discard_false_and_requests_history_discard_false(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_DATA_URI + "/configuration?discard=false&discardRequestsHistory=false")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": "true", "storeEventsRequestsHistory": "true" }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

