import pytest
import json


@pytest.mark.admin
def test_001_i_want_to_identify_valid_server_matching_operation(resources, h2ac_admin):

  requestBody = resources("server_matching_OK.json")
  responseBodyRef = { "result":"true", "response":"server_matching operation; valid schema and matching data received" }

  # Send POST
  response = h2ac_admin.post("/provision/v1/server_matching", requestBody)

  # Verify response
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

@pytest.mark.admin
def test_002_i_want_to_identify_wrong_schema_for_server_matching_operation(resources, h2ac_admin):

  requestBody = resources("server_matching_SCHEMA_NOK.json")
  responseBodyRef = { "result":"false", "response":"server_matching operation; invalid schema" }

  # Send POST
  response = h2ac_admin.post("/provision/v1/server_matching", requestBody)

  # Verify response
  h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)
  #assert response["status"] == 400
  #assert response["body"]["result"] == "false"
  #assert response["body"]["response"] == "server_matching operation; invalid schema"

@pytest.mark.admin
def test_003_i_want_to_identify_wrong_content_for_server_matching_operation(resources, h2ac_admin):

  requestBody1 = resources("server_matching_CONTENT_NOK1.json")
  requestBody2 = resources("server_matching_CONTENT_NOK2.json")
  requestBody3 = resources("server_matching_CONTENT_NOK3.json")
  responseBodyRef = { "result":"false", "response":"server_matching operation; invalid matching data received" }

  for requestBody in requestBody1, requestBody2, requestBody3:

    # Send POST
    response = h2ac_admin.post("/provision/v1/server_matching", requestBody)

    # Verify response
    h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)

