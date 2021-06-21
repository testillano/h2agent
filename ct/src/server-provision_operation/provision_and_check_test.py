import pytest
import json


@pytest.mark.admin
def test_001_cleanup_provisions(resources, h2ac_admin):
  response = h2ac_admin.delete("/provision/v1/server-provisions")


@pytest.mark.admin
def test_002_i_want_to_identify_wrong_schema_for_server_provision_operation_on_admin_interface(resources, h2ac_admin):

  requestBody = resources("server-provision_SCHEMA_NOK.json")
  responseBodyRef = { "result":"false", "response":"server-provision operation; invalid schema" }

  # Send POST
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)

  # Verify response
  h2ac_admin.assert_response__status_body_headers(response, 400, responseBodyRef)
  #assert response["status"] == 400
  #assert response["body"]["result"] == "false"
  #assert response["body"]["response"] == "server-provision operation; invalid schema"

@pytest.mark.admin
def test_003_i_want_to_identify_valid_server_provision_operation_on_admin_interface(resources, h2ac_admin):

  requestBody = resources("server-provision_OK.json.in").format(id="1")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }

  # Send POST
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)

  # Verify response
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

@pytest.mark.admin
def test_004_i_want_to_send_get_for_unsupported_operation_on_admin_interface(resources, h2ac_admin):

  # Send GET
  response = h2ac_admin.get("/provision/v1/foo")

  # Verify response
  assert response["status"] == 400

@pytest.mark.server
def test_005_i_want_to_send_get_request_for_provisioned_data_on_traffic_interface(resources, h2ac_admin, h2ac_traffic):

  # Send POST to configure FullMatching algorithm
  requestBody = resources("server-matching_OK1.json")
  response = h2ac_admin.post("/provision/v1/server-matching", requestBody)
  responseBodyRef = { "result":"true", "response":"server-matching operation; valid schema and matching data received" }
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/1")

  # Verify response
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

@pytest.mark.server
def test_006_i_want_to_send_get_request_for_non_provisioned_data_on_traffic_interface(resources, h2ac_traffic):

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/2")

  # Verify response
  h2ac_traffic.assert_response__status_body_headers(response, 501, "")

@pytest.mark.admin
def test_007_i_want_to_send_delete_server_provision_operations_on_admin_interface(resources, h2ac_admin):

  # Send DELETE
  response = h2ac_admin.delete("/provision/v1/server-provisions")

  # Verify response
  assert response["status"] == 200

  # Send DELETE again
  response = h2ac_admin.delete("/provision/v1/server-provisions")

  # Verify response
  assert response["status"] == 204 # no content as was removed at first DELETE

@pytest.mark.admin
def test_008_i_want_to_send_delete_for_unsupported_operation_on_admin_interface(resources, h2ac_admin):

  # Send DELETE
  response = h2ac_admin.delete("/provision/v1/foo")

  # Verify response
  assert response["status"] == 400

@pytest.mark.server
def test_009_i_want_to_check_fullmatchingregexreplace_on_traffic_interface(resources, h2ac_admin, h2ac_traffic):

  # Send POST to configure FullMatchingRegexReplace algorithm
  requestBody = resources("server-matching_FullMatchingRegexReplace.json")
  response = h2ac_admin.post("/provision/v1/server-matching", requestBody)
  responseBodyRef = { "result":"true", "response":"server-matching operation; valid schema and matching data received" }
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Provision
  requestBody = resources("server-provision_OK.json.in").format(id="1")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/1/ts-12345")
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

@pytest.mark.server
def test_010_i_want_to_check_prioritymatchingregex_on_traffic_interface(resources, h2ac_admin, h2ac_traffic):

  # Send POST to configure PriorityMatchingRegex algorithm
  requestBody = resources("server-matching_PriorityMatchingRegex.json")
  response = h2ac_admin.post("/provision/v1/server-matching", requestBody)
  responseBodyRef = { "result":"true", "response":"server-matching operation; valid schema and matching data received" }
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Provisions
  for prefix in [55500, 5551122, 555112244]:
    requestBody = resources("server-provision_PriorityMatchingRegex_{}.json".format(prefix))
    responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
    response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
    h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET
  response = h2ac_traffic.get("/app/v1/id-555112244/ts-1615562841")
  responseBodyRef = { "foo":"bar-5551122" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

@pytest.mark.server
def test_011_i_want_to_get_answer_for_default_provision_on_traffic_interface(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("default_GET_provision.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar/this-is-not-provisioned")

  # Verify response
  responseBodyRef = { "foo":"default", "bar":"default" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

@pytest.mark.admin
def test_012_multiple_provisions_operation(resources, h2ac_admin):

  # Provisions after initial cleanup
  response = h2ac_admin.delete("/provision/v1/server-provisions")
  requestBody = resources("server-provision_two_provisions_array.json")
  responseBodyRef = { "result":"true", "response":"server-provisions operation; valid schemas and provisions data received" }
  response = h2ac_admin.post("/provision/v1/server-provisions", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET
  response = h2ac_admin.get("/provision/v1/server-provisions")

  # Verify response
  responseBodyRef = [{"requestMethod":"GET","requestUri":"/app/v1/foo/bar/1","responseBody":{"foo":"bar-1"},"responseCode":200,"responseHeaders":{"content-type":"text/html","x-version":"1.0.0"}},{"requestMethod":"GET","requestUri":"/app/v1/foo/bar/2","responseBody":{"foo":"bar-2"},"responseCode":200,"responseHeaders":{"content-type":"text/html","x-version":"1.0.0"}}]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

