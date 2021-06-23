import pytest
import json


@pytest.mark.admin
def test_001_cleanup_provisions(resources, h2ac_admin):
  response = h2ac_admin.delete("/admin/v1/server-provisions")


@pytest.mark.admin
def test_002_i_want_to_get_internal_data_on_admin_interface(resources, h2ac_admin, h2ac_traffic):

  # Send GET
  response = h2ac_admin.get("/admin/v1/server-data")
  assert response["status"] == 204

  # Now we provision and send request for that provision (if no provision is
  # processed, then no internal data is stored for the request received):
  requestBody = resources("server-provision_OK.json.in").format(id=1)
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  # Verify response (status code is enough)
  assert response["status"] == 201

  # Send traffic request
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Send GET
  response = h2ac_admin.get("/admin/v1/server-data")
  responseBodyRef = [{'method': 'GET', 'requests': [{'previousState': 'initial', 'receptionTimestampMs': 1623552535012, 'responseBody': {'foo': 'bar-1'}, 'responseDelayMs': 0, 'responseHeaders': {'content-type': 'text/html', 'x-version': '1.0.0'}, 'responseStatusCode': 200, 'serverSequence': 32, 'state': 'initial'}], 'uri': '/app/v1/foo/bar/1'}]

  # Response will be something like: [{"method":"GET","requests":[{"body":{"foo":"bar-1"},"receptionTimestampMs":1623439423163,"state":"initial"}],"uri":"/app/v1/foo/bar/1"}]
  # We have a variable field 'receptionTimestampMs' and we don't know its real value.
  # So, we will remove it from both reference and response python dictionaries:
  # In this test we request all the server-data, so we must access the first array element with [0]:
  del response["body"][0]["requests"][0]["serverSequence"]
  del response["body"][0]["requests"][0]["receptionTimestampMs"]
  del responseBodyRef[0]["requests"][0]["serverSequence"]
  del responseBodyRef[0]["requests"][0]["receptionTimestampMs"]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_003_i_want_to_get_speficic_internal_data_on_admin_interface(resources, h2ac_admin, h2ac_traffic):

  # Do a second provision:
  requestBody = resources("server-provision_OK.json.in").format(id=5)
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  # Verify response (status code is enough)
  assert response["status"] == 201

  # Send traffic request
  response = h2ac_traffic.get("/app/v1/foo/bar/5")
  responseBodyRef = { "foo":"bar-5" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Send GET
  response = h2ac_admin.get("/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5")
  responseBodyRef = {'method': 'GET', 'requests': [{'previousState': 'initial', 'receptionTimestampMs': 1623552779114, 'responseBody': {'foo': 'bar-5'}, 'responseDelayMs': 0, 'responseHeaders': {'content-type': 'text/html', 'x-version': '1.0.0'}, 'responseStatusCode': 200, 'serverSequence': 35, 'state': 'initial'}], 'uri': '/app/v1/foo/bar/5'}

  # Response will be something like: {"method":"GET","requests":[{"body":{"foo":"bar-5"},"receptionTimestampMs":1623439631262,"state":"initial"}
  # We have a variable field 'receptionTimestampMs' and we don't know its real value.
  # So, we will remove it from both reference and response python dictionaries:
  # In this test we request specific server-data, so we don't have array:
  del response["body"]["requests"][0]["serverSequence"]
  del response["body"]["requests"][0]["receptionTimestampMs"]
  del responseBodyRef["requests"][0]["serverSequence"]
  del responseBodyRef["requests"][0]["receptionTimestampMs"]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

