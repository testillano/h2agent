import pytest
import json
from conftest import BASIC_FOO_BAR_SERVER_PROVISION_TEMPLATE, string2dict, ADMIN_SERVER_DATA_URI


@pytest.mark.admin
def test_001_i_want_to_set_discard_true_and_key_history_discard_true(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?discard=true&discardKeyHistory=true")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": False, "storeEventsKeyHistory": False, "purgeExecution": True }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_002_i_want_to_set_discard_true_and_key_history_discard_false(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?discard=true&discardKeyHistory=false")
  assert response["status"] == 400

  # Check configuration (must be the configuration from previous test, as this one failed):
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": False, "storeEventsKeyHistory": False, "purgeExecution": True }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_003_i_want_to_set_discard_false_and_key_history_discard_true(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?discard=false&discardKeyHistory=true")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": True, "storeEventsKeyHistory": False, "purgeExecution": True }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_004_i_want_to_set_discard_false_and_key_history_discard_false(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?discard=false&discardKeyHistory=false")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": True, "storeEventsKeyHistory": True, "purgeExecution": True }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_005_i_want_to_set_disable_purge_true(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?disablePurge=true")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": True, "storeEventsKeyHistory": True, "purgeExecution": False }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


# Last test to restore default purge configuration: enabled
@pytest.mark.admin
def test_006_i_want_to_set_disable_purge_false(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?disablePurge=false")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/configuration")
  responseBodyRef = { "storeEvents": True, "storeEventsKeyHistory": True, "purgeExecution": True }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_007_i_want_to_set_invalid_server_data_configuration(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_DATA_URI + "/configuration?foo=bar")
  assert response["status"] == 400

