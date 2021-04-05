import pytest


@pytest.mark.admin
def test_001_i_want_to_force_bad_request_error_due_to_json_parse_error(h2ac):

  requestBody = "foo"
  responseBodyRef = { "result":"false", "response":"failed to parse json from body request" }

  # Send POST
  response = h2ac.post("/provision/v1", requestBody)

  # Verify response
  h2ac.assert_response__status_body_headers(response, 400, responseBodyRef)
