import pytest


# No need for cleanup


@pytest.mark.admin
def test_001_i_want_to_force_unsupported_media_type_error_towards_h2agent_admin_api(h2ac_admin):

  requestBody = "foo"
  #responseBodyRef = { "cause":"UNSUPPORTED_MEDIA_TYPE" }

  # Send POST
  response = h2ac_admin.postDict("/admin/v1", requestBody, requestHeaders={'content-type': 'application/text'})

  # Verify response
  #h2ac_admin.assert_response__status_body_headers(response, 415, responseBodyRef)
  assert response["status"] == 415
  assert response["body"]["cause"] == "UNSUPPORTED_MEDIA_TYPE"
  assert response["headers"]["content-type"] == [b'application/problem+json']

  #mylogger.debug("\nRESPONSE: \n\n" + str(response))

