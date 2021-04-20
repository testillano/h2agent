import pytest


@pytest.mark.admin
def test_001_i_want_to_force_bad_request_error_due_to_json_parse_error(h2ac_admin):

  requestBody = "foo"
  responseBodyRef = { "result":"false", "response":"failed to parse json from body request" }

  # Send POST
  response = h2ac_admin.post("/provision/v1/bar", requestBody)

  # Verify response
  h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)

@pytest.mark.admin
def test_002_i_want_to_force_bad_request_error_due_to_no_uri_operation_provided(h2ac_admin):

  requestBody = "foo"
  responseBodyRef = { "result":"false", "response":"no operation provided" }

  # Send POST
  response = h2ac_admin.post("/provision/v1", requestBody)

  # Verify response
  h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)

