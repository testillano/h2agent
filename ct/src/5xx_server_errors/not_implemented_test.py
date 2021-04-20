import pytest
import json


@pytest.mark.admin
def test_001_i_want_to_force_not_implemented_error_towards_h2agent_admin_api(h2ac_admin):

  requestBody = { "foo": "bar" }
  #responseBodyRef = { "cause":"METHOD_NOT_ALLOWED" }

  # Send POST
  requestBodyJson = json.dumps(requestBody, indent=None, separators=(',', ':'))
  response = h2ac_admin.request('GET', "/provision/v1", requestBodyJson)

  # Verify response
  #h2ac_admin.assert_response__status_body_headers(response, 415, responseBodyRef)
  assert response["status"] == 501
  assert response["body"]["cause"] == "METHOD_NOT_IMPLEMENTED"
  assert response["headers"]["content-type"] == [b'application/problem+json']

