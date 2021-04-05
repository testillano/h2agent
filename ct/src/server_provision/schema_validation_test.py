import pytest
import json


@pytest.mark.admin
def test_001_i_want_to_identify_wrong_schema_for_server_provision_operation(resources, h2ac):

  requestBody = resources("server_provision_NOK.json")
  responseBodyRef = { "result":"false", "response":"server_provision operation; invalid schema" }

  # Send POST
  response = h2ac.post("/provision/v1/server_provision", requestBody)

  # Verify response
  h2ac.assert_response__status_body_headers(response, 400, responseBodyRef)
  #assert response["status"] == 400
  #assert response["body"]["result"] == "false"
  #assert response["body"]["response"] == "server_provision operation; invalid schema"


@pytest.mark.admin
def test_002_i_want_to_identify_valid_schema_for_server_provision_operation(resources, h2ac):

  requestBody = resources("server_provision_OK.json")
  responseBodyRef = { "result":"true", "response":"server_provision operation; valid schema" }

  # Send POST
  response = h2ac.post("/provision/v1/server_provision", requestBody)

  # Verify response
  h2ac.assert_response__status_body_headers(response, 201, responseBodyRef)

