import pytest
import json

@pytest.mark.admin
def test_001_i_want_to_get_internal_data_on_admin_interface(resources, h2ac_admin, h2ac_traffic):

  # Send GET
  response = h2ac_admin.get("/provision/v1/server-data")
  assert response["status"] == 204

  # Now we provision and send request for that provision (if no provision is
  # processed, then no internal data is stored for the request received):
  requestBody = resources("server-provision_OK.json.in").format(id=1)
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  # Verify response (status code is enough)
  assert response["status"] == 201

  # Send traffic request
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Send GET
  response = h2ac_admin.get("/provision/v1/server-data")
  responseBodyRef = [{'body': {'foo': 'bar-1'}, 'method': 'GET', 'state': 'initial', 'uri': '/app/v1/foo/bar/1'}]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

@pytest.mark.admin
def test_002_i_want_to_get_speficic_internal_data_on_admin_interface(resources, h2ac_admin, h2ac_traffic):

  # Do a second provision:
  requestBody = resources("server-provision_OK.json.in").format(id=5)
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  # Verify response (status code is enough)
  assert response["status"] == 201

  # Send traffic request
  response = h2ac_traffic.get("/app/v1/foo/bar/5")
  responseBodyRef = { "foo":"bar-5" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Send GET
  response = h2ac_admin.get("/provision/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5")
  responseBodyRef = {'body': {'foo': 'bar-5'}, 'method': 'GET', 'state': 'initial', 'uri': '/app/v1/foo/bar/5'}
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

@pytest.mark.admin
def test_003_cleanup_provisions(resources, h2ac_admin):

  # Send DELETE
  response = h2ac_admin.delete("/provision/v1/server-provision")

  # Verify response
  assert response["status"] == 200

