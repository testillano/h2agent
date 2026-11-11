import pytest
import json
from conftest import SCHEMAS_SERVER_PROVISION_TEMPLATE, MY_REQUESTS_SCHEMA_ID_TEMPLATE, string2dict, ADMIN_SCHEMA_URI, ADMIN_SERVER_DATA_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_configure_schemas_and_corresponding_provision_on_admin_interface(admin_server_provision, admin_schema):

  admin_server_provision(string2dict(SCHEMAS_SERVER_PROVISION_TEMPLATE, reqId="myRequestsSchemaId", resId="myResponsesSchemaId", responseBodyField="bar"))
  admin_schema(string2dict(MY_REQUESTS_SCHEMA_ID_TEMPLATE, id="myRequestsSchemaId", requiredProperty="foo"))
  admin_schema(string2dict(MY_REQUESTS_SCHEMA_ID_TEMPLATE, id="myResponsesSchemaId", requiredProperty="bar"))


@pytest.mark.admin
def test_002_i_want_to_retrieve_current_schemas_on_admin_interface(h2ac_admin):

  # Send GET
  response = h2ac_admin.get(ADMIN_SCHEMA_URI)

  # Verify response
  assert response["status"] == 200
  response0 = response["body"][0]
  response1 = response["body"][1]
  id0 = response0["id"]
  id1 = response1["id"]
  schemaReq = string2dict(MY_REQUESTS_SCHEMA_ID_TEMPLATE, id="myRequestsSchemaId", requiredProperty="foo")
  schemaRes = string2dict(MY_REQUESTS_SCHEMA_ID_TEMPLATE, id="myResponsesSchemaId", requiredProperty="bar")
  if id0 == "myRequestsSchemaId":
    assert response0 == schemaReq
    assert response1 == schemaRes
  else:
    assert response0 == schemaRes
    assert response1 == schemaReq


def test_003_i_want_to_send_valid_post_request_for_provisioned_data_on_traffic_interface(h2ac_traffic):

  # Send POST
  response = h2ac_traffic.postDict("/app/v1/foo/bar", { "foo": "required" })

  # Verify response
  responseBodyRef = { "bar":"test" } # responseBodyField="bar" (see at test_001 provision)
  h2ac_traffic.assert_response__status_body_headers(response, 201, responseBodyRef)


def test_004_i_want_to_send_post_request_for_provisioned_data_on_traffic_interface_and_fail_on_request_schema(h2ac_traffic, h2ac_admin):

  # Send POST
  response = h2ac_traffic.postDict("/app/v1/foo/bar", { "foo": "required", "no-additional-allowed": "test" })

  # Verify response
  h2ac_traffic.assert_response__status_body_headers(response, 400, "") # failed on request schema, so no transformations and response body are processed

  # The failure is a state of the key (it does not advance to the provision outState), and its detail is in the event:
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/foo/bar&eventNumber=-1")
  assert response["status"] == 200
  event = response["body"]
  assert event["previousState"] == "initial"
  assert event["state"] == "__core:failed:initial:requestSchemaId"
  assert len(event["failures"]) == 1
  assert event["failures"][0]["reason"] == "requestSchemaId"
  assert event["failures"][0]["id"] == "myRequestsSchemaId"

  # No server provision has that inState, so the key stays there (next request: 501) until its events are removed:
  response = h2ac_traffic.postDict("/app/v1/foo/bar", { "foo": "required" })
  assert response["status"] == 501
  h2ac_admin.delete(ADMIN_SERVER_DATA_URI)


def test_005_i_want_to_send_post_request_for_provisioned_data_on_traffic_interface_and_fail_on_response_schema(admin_server_provision, h2ac_traffic):

  # Update provision
  admin_server_provision(string2dict(SCHEMAS_SERVER_PROVISION_TEMPLATE, reqId="myRequestsSchemaId", resId="myResponsesSchemaId", responseBodyField="bad-response-field"))

  # Send POST
  response = h2ac_traffic.postDict("/app/v1/foo/bar", { "foo": "required" })

  # Verify response
  responseBodyRef = { "bad-response-field":"test" } # see updated provision above
  h2ac_traffic.assert_response__status_body_headers(response, 500, responseBodyRef) # bad response schema will override status code to internal server error

