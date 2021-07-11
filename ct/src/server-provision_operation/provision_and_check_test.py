import pytest
import json
from conftest import ADMIN_PROVISION_URI, INVALID_PROVISION_SCHEMA__RESPONSE_BODY, VALID_PROVISIONS__RESPONSE_BODY
from conftest import string2dict, BASIC_FOO_BAR_PROVISION_TEMPLATE, REGEX_FOO_BAR_PROVISION_TEMPLATE, FALLBACK_DEFAULT_PROVISION


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_identify_wrong_schema_for_server_provision_operation_on_admin_interface(admin_provision):

  admin_provision({ "foo": "bar" }, responseBodyRef = INVALID_PROVISION_SCHEMA__RESPONSE_BODY, responseStatusRef = 400)


@pytest.mark.admin
def test_002_i_want_to_identify_valid_server_provision_operation_on_admin_interface(admin_provision):

  admin_provision(string2dict(BASIC_FOO_BAR_PROVISION_TEMPLATE, id=1))


@pytest.mark.admin
def test_003_i_want_to_send_get_for_unsupported_operation_on_admin_interface(h2ac_admin):

  # Send GET
  response = h2ac_admin.get("/admin/v1/foo")

  # Verify response
  assert response["status"] == 400


@pytest.mark.server
def test_004_i_want_to_send_get_request_for_provisioned_data_on_traffic_interface(h2ac_admin, h2ac_traffic):

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/1")

  # Verify response
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.server
def test_005_i_want_to_send_get_request_for_non_provisioned_data_on_traffic_interface(h2ac_traffic):

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/2")

  # Verify response
  h2ac_traffic.assert_response__status_body_headers(response, 501, "")


@pytest.mark.admin
def test_006_i_want_to_send_delete_server_provision_operations_on_admin_interface(h2ac_admin):

  # Send DELETE
  response = h2ac_admin.delete(ADMIN_PROVISION_URI)

  # Verify response
  assert response["status"] == 200

  # Send DELETE again
  response = h2ac_admin.delete(ADMIN_PROVISION_URI)

  # Verify response
  assert response["status"] == 204 # no content as was removed at first DELETE


@pytest.mark.admin
def test_007_i_want_to_send_delete_for_unsupported_operation_on_admin_interface(h2ac_admin):

  # Send DELETE
  response = h2ac_admin.delete("/admin/v1/foo")

  # Verify response
  assert response["status"] == 400


@pytest.mark.server
def test_008_i_want_to_check_fullmatchingregexreplace_on_traffic_interface(admin_matching, admin_provision, h2ac_traffic):

  # Configure FullMatchingRegexReplace algorithm
  algorithm = { "algorithm":"FullMatchingRegexReplace", "rgx":"(/app/v1/foo/bar/[0-9]+)/ts-([0-9]+)", "fmt":"$1" }
  admin_matching(algorithm)

  # Provision
  admin_provision(string2dict(BASIC_FOO_BAR_PROVISION_TEMPLATE, id=1))

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/1/ts-12345")
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.server
def test_009_i_want_to_check_prioritymatchingregex_on_traffic_interface(admin_cleanup, admin_provision, h2ac_traffic):

  # Cleanup and set PriorityMatchingRegex matching algorithm:
  admin_cleanup(matchingContent={ "algorithm":"PriorityMatchingRegex" })

  # Provisions
  admin_provision(string2dict(REGEX_FOO_BAR_PROVISION_TEMPLATE, id=55500, trailing=4))
  admin_provision(string2dict(REGEX_FOO_BAR_PROVISION_TEMPLATE, id=5551122, trailing=2))
  admin_provision(string2dict(REGEX_FOO_BAR_PROVISION_TEMPLATE, id=555112244, trailing=0))

  # Send GET
  response = h2ac_traffic.get("/app/v1/id-555112244/ts-1615562841")
  responseBodyRef = { "foo":"bar-5551122" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.server
def test_010_i_want_to_get_answer_for_default_provision_on_traffic_interface(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(FALLBACK_DEFAULT_PROVISION))

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/this-is-not-explicitly-provisioned")

  # Verify response
  responseBodyRef = { "foo":"default", "bar":"default" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_011_multiple_provisions_operation(admin_cleanup, admin_provision, h2ac_admin):

  # Provisions from scratch, and force order for provisions check:
  admin_cleanup(matchingContent={ "algorithm":"PriorityMatchingRegex" })

  provisions = [{"requestMethod":"GET","requestUri":"/app/v1/foo/bar/1","responseBody":{"foo":"bar-1"},"responseCode":200,"responseHeaders":{"content-type":"text/html","x-version":"1.0.0"}},{"requestMethod":"GET","requestUri":"/app/v1/foo/bar/2","responseBody":{"foo":"bar-2"},"responseCode":200,"responseHeaders":{"content-type":"text/html","x-version":"1.0.0"}}]
  admin_provision(provisions, responseBodyRef=VALID_PROVISIONS__RESPONSE_BODY)

  # Check provisions
  response = h2ac_admin.get(ADMIN_PROVISION_URI)
  h2ac_admin.assert_response__status_body_headers(response, 200, provisions)

