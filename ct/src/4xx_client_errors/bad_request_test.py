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

