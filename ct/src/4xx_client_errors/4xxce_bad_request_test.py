import pytest


# No need for cleanup


@pytest.mark.admin
def test_001_i_want_to_force_bad_request_error_towards_h2agent_admin_api(h2ac_admin):

  requestBody = { "foo": "bar" }
  #responseBodyRef = { "cause":"INVALID_API" }

  # Send POST
  response = h2ac_admin.postDict("/addMiinn/v1", requestBody)

  # Verify response
  #h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)
  assert response["status"] == 400
  assert response["body"]["cause"] == "INVALID_API"
  assert response["headers"]["content-type"] == [b'application/problem+json']


@pytest.mark.admin
def test_002_i_want_to_force_bad_request_error_towards_h2agent_traffic_api(h2ac_traffic):

  requestBody = { "foo": "bar" }
  #responseBodyRef = { "cause":"INVALID_API" }

  # Send POST
  response = h2ac_traffic.get("/aaappp/v0/the/path")

  # Verify response
  #h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)
  assert response["status"] == 400
  assert response["body"]["cause"] == "INVALID_API"
  assert response["headers"]["content-type"] == [b'application/problem+json']


@pytest.mark.admin
def test_003_i_want_to_get_gone_with_hint_when_using_the_retired_admin_v1_api(h2ac_admin):

  # GET on a retired version: 410 pointing to the same resource on the current version
  response = h2ac_admin.get("/admin/v1/configuration")
  assert response["status"] == 410
  assert "/admin/v2/configuration" in response["body"]["cause"]
  assert "migrate" in response["body"]["cause"].lower()
  assert response["headers"]["content-type"] == [b'application/problem+json']

  # Any method, any resource (and the bare version path)
  response = h2ac_admin.postDict("/admin/v1/server-provision", { "foo": "bar" })
  assert response["status"] == 410
  response = h2ac_admin.get("/admin/v1")
  assert response["status"] == 410

  # Not a retired version: generic invalid API, unchanged
  response = h2ac_admin.get("/admin/v1x/configuration")
  assert response["status"] == 400
  assert response["body"]["cause"] == "INVALID_API"
