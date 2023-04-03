import pytest
import json
from conftest import BASIC_FOO_BAR_SERVER_PROVISION_TEMPLATE, string2dict, ADMIN_SERVER_DATA_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_get_internal_data_on_admin_interface(h2ac_admin, h2ac_traffic, admin_server_provision):

  # Check that there is no server data here
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  assert response["status"] == 204

  # Provision and traffic
  admin_server_provision(string2dict(BASIC_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1))
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check server data
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  responseBodyRef = [{'method': 'GET', 'events': [{'previousState': 'initial', 'receptionTimestampUs': 1623552535012124, 'responseBody': {'foo': 'bar-1'}, 'responseDelayMs': 0, 'responseHeaders': {'content-type': 'application/json', 'x-version': '1.0.0'}, 'responseStatusCode': 200, 'serverSequence': 32, 'state': 'initial'}], 'uri': '/app/v1/foo/bar/1'}]
  # Response will be something like: [{"method":"GET","events":[{"responseBody":{"foo":"bar-1"},"receptionTimestampUs":1623439423163143,"state":"initial"}],"uri":"/app/v1/foo/bar/1"}]
  # We have a variable field 'receptionTimestampUs' and we don't know its real value.
  # So, we will remove it from both reference and response python dictionaries:
  # In this test we request all the server-data, so we must access the first array element with [0]:
  del response["body"][0]["events"][0]["serverSequence"]
  del response["body"][0]["events"][0]["receptionTimestampUs"]
  del responseBodyRef[0]["events"][0]["serverSequence"]
  del responseBodyRef[0]["events"][0]["receptionTimestampUs"]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_002_i_want_to_get_speficic_internal_data_on_admin_interface(h2ac_admin, h2ac_traffic, admin_server_provision):

  # Do a second provision:
  admin_server_provision(string2dict(BASIC_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=2))
  response = h2ac_traffic.get("/app/v1/foo/bar/2")
  responseBodyRef = { "foo":"bar-2" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check server data
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar/2")
  responseBodyRef = { "method": "GET", "events": [{'previousState': 'initial', 'responseBody': {'foo': 'bar-2'}, 'responseDelayMs': 0, 'responseHeaders': {'content-type': 'application/json', 'x-version': '1.0.0'}, 'responseStatusCode': 200, 'state': 'initial'}], "uri": "/app/v1/foo/bar/2" }
  # serverSequence and receptionTimestampUs have been removed from reference and also will be removed from response as they are not predictable:
  # hyper does not add headers on traffic as curl does, so we don't have to remove 'headers' key.
  del response["body"]["events"][0]["serverSequence"] # depends on the server sequence since the h2agent was started
  del response["body"]["events"][0]["receptionTimestampUs"] # completely unpredictable
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_003_i_want_to_get_speficic_internal_data_on_admin_interface_using_eventNumber(h2ac_admin, h2ac_traffic, admin_server_provision):

  # Get again
  response = h2ac_traffic.get("/app/v1/foo/bar/2")
  responseBodyRef = { "foo":"bar-2" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check server data
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar/2&eventNumber=-1")
  responseBodyRef = { 'previousState': 'initial', 'responseBody': {'foo': 'bar-2'}, 'responseDelayMs': 0, 'responseHeaders': {'content-type': 'application/json', 'x-version': '1.0.0'}, 'responseStatusCode': 200, 'state': 'initial'}
  # serverSequence and receptionTimestampUs have been removed from reference and also will be removed from response as they are not predictable:
  # hyper does not add headers on traffic as curl does, so we don't have to remove 'headers' key.
  del response["body"]["serverSequence"] # depends on the server sequence since the h2agent was started
  del response["body"]["receptionTimestampUs"] # completely unpredictable
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_004_i_want_to_get_speficic_internal_data_on_admin_interface_using_eventNumber_and_eventPath(h2ac_admin, h2ac_traffic, admin_server_provision):

  # Get again
  response = h2ac_traffic.get("/app/v1/foo/bar/2")
  responseBodyRef = { "foo":"bar-2" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check server data
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar/2&eventNumber=-1&eventPath=/responseBody")
  responseBodyRef = {'foo': 'bar-2'}
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

