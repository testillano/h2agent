import pytest
import json


@pytest.mark.admin
def test_001_cleanup_provisions(resources, h2ac_admin):
  response = h2ac_admin.delete("/admin/v1/server-provision")


@pytest.mark.admin
def test_002_i_want_to_identify_wrong_schema_for_server_matching_operation_on_admin_interface(resources, h2ac_admin):

  requestBody = resources("server-matching_SCHEMA_NOK.json")
  responseBodyRef = { "result":"false", "response":"server-matching operation; invalid schema" }

  # Send POST
  response = h2ac_admin.post("/admin/v1/server-matching", requestBody)

  # Verify response
  h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)
  #assert response["status"] == 400
  #assert response["body"]["result"] == "false"
  #assert response["body"]["response"] == "server-matching operation; invalid schema"

@pytest.mark.admin
def test_003_i_want_to_identify_wrong_content_for_server_matching_operation_on_admin_interface(resources, h2ac_admin):

  requestBody1 = resources("server-matching_CONTENT_NOK1.json")
  requestBody2 = resources("server-matching_CONTENT_NOK2.json")
  requestBody3 = resources("server-matching_CONTENT_NOK3.json")
  responseBodyRef = { "result":"false", "response":"server-matching operation; invalid matching data received" }

  for requestBody in requestBody1, requestBody2, requestBody3:

    # Send POST
    response = h2ac_admin.post("/admin/v1/server-matching", requestBody)

    # Verify response
    h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)

@pytest.mark.admin
def test_004_i_want_to_send_valid_server_matching_operations_on_admin_interface(resources, h2ac_admin):

  # Reference for POST configuration
  responseBodyRef = { "result":"true", "response":"server-matching operation; valid schema and matching data received" }

  # FullMatching:

  # Send POST to configure
  requestBody = resources("server-matching_OK1.json")
  response = h2ac_admin.post("/admin/v1/server-matching", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET to check configuration
  response = h2ac_admin.get("/admin/v1/server-matching")
  h2ac_admin.assert_response__status_body_headers(response, 200, { "algorithm":"FullMatching", "uriPathQueryParametersFilter":"PassBy" })

  # FullMatchingRegexReplace:

  # Send POST to configure
  requestBody = resources("server-matching_OK2.json")
  response = h2ac_admin.post("/admin/v1/server-matching", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET to check configuration
  response = h2ac_admin.get("/admin/v1/server-matching")
  h2ac_admin.assert_response__status_body_headers(response, 200, { "algorithm":"FullMatchingRegexReplace", "rgx":"", "fmt":"" })


@pytest.mark.admin
def test_005_i_set_default_server_matching_on_admin_interface(resources, h2ac_admin):

  # Reference for POST configuration
  responseBodyRef = { "result":"true", "response":"server-matching operation; valid schema and matching data received" }

  # FullMatching:

  # Send POST to configure
  requestBody = { "algorithm":"FullMatching" }
  response = h2ac_admin.postDict("/admin/v1/server-matching", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET to check configuration
  response = h2ac_admin.get("/admin/v1/server-matching")
  h2ac_admin.assert_response__status_body_headers(response, 200, requestBody)

