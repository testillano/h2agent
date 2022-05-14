import pytest
import json
from conftest import INVALID_MATCHING_SCHEMA__RESPONSE_BODY, INVALID_MATCHING_DATA__RESPONSE_BODY, ADMIN_SERVER_MATCHING_URI


# No need for cleanup


@pytest.mark.admin
def test_001_i_want_to_identify_wrong_schema_for_server_matching_operation_on_admin_interface(admin_server_matching):

  # Matching
  admin_server_matching({ "foo": "bar" }, responseBodyRef = INVALID_MATCHING_SCHEMA__RESPONSE_BODY, responseStatusRef = 400)


@pytest.mark.admin
def test_002_i_want_to_identify_wrong_content_for_server_matching_operation_on_admin_interface(admin_server_matching):

  admin_server_matching({ "algorithm":"FullMatching", "rgx":"whatever", "fmt":"whatever" }, responseBodyRef = INVALID_MATCHING_DATA__RESPONSE_BODY, responseStatusRef = 400)
  admin_server_matching({ "algorithm":"PriorityMatchingRegex", "rgx":"whatever", "fmt":"whatever" }, responseBodyRef = INVALID_MATCHING_DATA__RESPONSE_BODY, responseStatusRef = 400)
  admin_server_matching({ "algorithm":"FullMatchingRegexReplace" }, responseBodyRef = INVALID_MATCHING_DATA__RESPONSE_BODY, responseStatusRef = 400)


@pytest.mark.admin
def test_003_i_want_to_send_and_check_valid_server_matching_operations_on_admin_interface(admin_server_matching, h2ac_admin):

  # FullMatching with passBy query parameters
  FullMatchingPassBy = { "algorithm":"FullMatching", "uriPathQueryParametersFilter":"PassBy" }
  admin_server_matching(FullMatchingPassBy)
  response = h2ac_admin.get(ADMIN_SERVER_MATCHING_URI)
  h2ac_admin.assert_response__status_body_headers(response, 200, FullMatchingPassBy)

  # FullMatchingRegexReplace
  FullMatchingRegexReplace = { "algorithm":"FullMatchingRegexReplace", "rgx":"", "fmt":"" }
  admin_server_matching(FullMatchingRegexReplace)
  response = h2ac_admin.get(ADMIN_SERVER_MATCHING_URI)
  h2ac_admin.assert_response__status_body_headers(response, 200, FullMatchingRegexReplace)

  # Default FullMatching:
  FullMatching = { "algorithm":"FullMatching" }
  admin_server_matching(FullMatching)
  response = h2ac_admin.get(ADMIN_SERVER_MATCHING_URI)
  h2ac_admin.assert_response__status_body_headers(response, 200, FullMatching)

