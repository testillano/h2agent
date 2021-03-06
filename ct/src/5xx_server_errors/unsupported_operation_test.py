import pytest


# No need for cleanup


@pytest.mark.admin
def test_001_i_want_to_force_unsupported_operation_towards_admin_interface(h2ac_admin):

  requestBody = { "foo":"bar" }
  responseBodyRef = { "result":"false", "response":"unsupported operation" }

  # Send POST
  response = h2ac_admin.postDict("/admin/v1/foo", requestBody)

  # Verify response
  h2ac_admin.assert_response__status_body_headers(response, 501, responseBodyRef)

