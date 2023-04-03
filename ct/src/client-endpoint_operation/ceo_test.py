import pytest
import json
from conftest import ADMIN_CLIENT_ENDPOINT_URI, INVALID_CLIENT_ENDPOINT_SCHEMA__RESPONSE_BODY, VALID_CLIENT_ENDPOINT__RESPONSE_BODY, VALID_CLIENT_ENDPOINTS__RESPONSE_BODY
from conftest import string2dict, CLIENT_ENDPOINT_TEMPLATE


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.server
def test_001_send_get_client_endpoint_when_nothing_is_configured(h2ac_admin):

  # Send GET
  response = h2ac_admin.get(ADMIN_CLIENT_ENDPOINT_URI)

  # Verify response
  assert response["status"] == 204


@pytest.mark.server
def test_002_configure_client_endpoint(h2ac_admin, admin_client_endpoint):

  # Configuration
  clientEndPoint = string2dict(CLIENT_ENDPOINT_TEMPLATE, id="foo", port=6788)
  admin_client_endpoint(clientEndPoint, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)

  # Check configuration
  response = h2ac_admin.get(ADMIN_CLIENT_ENDPOINT_URI)
  responseBodyRef = [ {'host':'0.0.0.0','id':'foo','permit':True,'port':6788,'secure':False} ]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.server
def test_003_remove_client_endpoints(h2ac_admin, admin_client_endpoint):

  # Send DELETE
  response = h2ac_admin.delete(ADMIN_CLIENT_ENDPOINT_URI)

  # Verify response
  assert response["status"] == 200


@pytest.mark.server
def test_004_configure_multiple_client_endpoints(h2ac_admin, admin_client_endpoint):

  # Configuration
  clientEndPoint1 = string2dict(CLIENT_ENDPOINT_TEMPLATE, id="foo", port=6788)
  clientEndPoint2 = string2dict(CLIENT_ENDPOINT_TEMPLATE, id="bar", port=6789)
  clientEndPoints = [ clientEndPoint1, clientEndPoint2 ]
  admin_client_endpoint(clientEndPoints, responseBodyRef=VALID_CLIENT_ENDPOINTS__RESPONSE_BODY)

  # Check configuration
  response = h2ac_admin.get(ADMIN_CLIENT_ENDPOINT_URI)
  responseBodyRef = [ {'host':'0.0.0.0','id':'bar','permit':True,'port':6789,'secure':False}, {'host':'0.0.0.0','id':'foo','permit':True,'port':6788,'secure':False} ]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

