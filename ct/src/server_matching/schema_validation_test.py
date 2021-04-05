import pytest
import json


@pytest.mark.admin
def test_001_i_want_to_identify_wrong_schema_for_server_matching_operation(resources, h2ac):

  requestBody = resources("server_matching_NOK.json")
  responseBodyRef = { "result":"false", "response":"server_matching operation; invalid schema" }

  # Send POST
  response = h2ac.post("/provision/v1/server_matching", requestBody)

  # Verify response
  h2ac.assert_response__status_body_headers(response, 400, responseBodyRef)
  #assert response["status"] == 400
  #assert response["body"]["result"] == "false"
  #assert response["body"]["response"] == "server_matching operation; invalid schema"


@pytest.mark.admin
def test_002_i_want_to_identify_valid_schema_for_server_matching_operation(resources, h2ac):

  requestBody = resources("server_matching_OK.json")
  responseBodyRef = { "result":"true", "response":"server_matching operation; valid schema" }

  # Send POST
  response = h2ac.post("/provision/v1/server_matching", requestBody)

  # Verify response
  h2ac.assert_response__status_body_headers(response, 201, responseBodyRef)

