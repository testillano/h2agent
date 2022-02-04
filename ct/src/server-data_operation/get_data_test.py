import pytest
import json
from conftest import BASIC_FOO_BAR_PROVISION_TEMPLATE, string2dict, ADMIN_DATA_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_get_internal_data_on_admin_interface(h2ac_admin, h2ac_traffic, admin_provision):

  # Check that there is no server data here
  response = h2ac_admin.get(ADMIN_DATA_URI)
  assert response["status"] == 204

  # Provision and traffic
  admin_provision(string2dict(BASIC_FOO_BAR_PROVISION_TEMPLATE, id=1))
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check server data
  response = h2ac_admin.get(ADMIN_DATA_URI)
  responseBodyRef = [{'method': 'GET', 'requests': [{'previousState': 'initial', 'receptionTimestampMs': 1623552535012, 'responseBody': {'foo': 'bar-1'}, 'responseDelayMs': 0, 'responseHeaders': {'content-type': 'text/html', 'x-version': '1.0.0'}, 'responseStatusCode': 200, 'serverSequence': 32, 'state': 'initial'}], 'uri': '/app/v1/foo/bar/1'}]
  # Response will be something like: [{"method":"GET","requests":[{"responseBody":{"foo":"bar-1"},"receptionTimestampMs":1623439423163,"state":"initial"}],"uri":"/app/v1/foo/bar/1"}]
  # We have a variable field 'receptionTimestampMs' and we don't know its real value.
  # So, we will remove it from both reference and response python dictionaries:
  # In this test we request all the server-data, so we must access the first array element with [0]:
  del response["body"][0]["requests"][0]["serverSequence"]
  del response["body"][0]["requests"][0]["receptionTimestampMs"]
  del responseBodyRef[0]["requests"][0]["serverSequence"]
  del responseBodyRef[0]["requests"][0]["receptionTimestampMs"]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_002_i_want_to_get_speficic_internal_data_on_admin_interface(h2ac_admin, h2ac_traffic, admin_provision):

  # Do a second provision:
  admin_provision(string2dict(BASIC_FOO_BAR_PROVISION_TEMPLATE, id=2))
  response = h2ac_traffic.get("/app/v1/foo/bar/2")
  responseBodyRef = { "foo":"bar-2" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check server data
  response = h2ac_admin.get(ADMIN_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar/2")
  responseBodyRef = { "method": "GET", "requests": [{'previousState': 'initial', 'responseBody': {'foo': 'bar-2'}, 'responseDelayMs': 0, 'responseHeaders': {'content-type': 'text/html', 'x-version': '1.0.0'}, 'responseStatusCode': 200, 'state': 'initial'}], "uri": "/app/v1/foo/bar/2" }
  # serverSequence and receptionTimestampMs have been removed from reference and also will be removed from response as they are not predictable:
  # hyper does not add headers on traffic as curl does, so we don't have to remove 'headers' key.
  del response["body"]["requests"][0]["serverSequence"] # depends on the server sequence since the h2agent was started
  del response["body"]["requests"][0]["receptionTimestampMs"] # completely unpredictable
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

