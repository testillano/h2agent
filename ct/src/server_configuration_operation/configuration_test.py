import pytest
import json
from conftest import ADMIN_SERVER_CONFIGURATION_URI


@pytest.mark.admin
def test_001_i_want_to_put_server_configuration(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_CONFIGURATION_URI + "?receiveRequestBody=false&preReserveRequestBody=false")
  assert response["status"] == 200

  # Check configuration
  response = h2ac_admin.get(ADMIN_SERVER_CONFIGURATION_URI)
  responseBodyRef = { "preReserveRequestBody": False , "receiveRequestBody": False }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Restore defaults: mainly receive body !!
  response = h2ac_admin.put(ADMIN_SERVER_CONFIGURATION_URI + "?receiveRequestBody=true&preReserveRequestBody=true")
  assert response["status"] == 200


@pytest.mark.admin
def test_002_i_want_to_get_server_configuration(h2ac_admin):

  # Check configuration
  response = h2ac_admin.get(ADMIN_SERVER_CONFIGURATION_URI)
  responseBodyRef = { "preReserveRequestBody": True , "receiveRequestBody": True }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_003_i_want_to_set_invalid_server_configuration(h2ac_admin):

  # Configure
  response = h2ac_admin.put(ADMIN_SERVER_CONFIGURATION_URI + "?foo=bar")
  assert response["status"] == 400

