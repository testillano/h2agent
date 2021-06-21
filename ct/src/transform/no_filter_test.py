import pytest
import json


@pytest.mark.transform
def test_001_cleanup_provisions(resources, h2ac_admin):
  response = h2ac_admin.delete("/provision/v1/server-provision")


@pytest.mark.transform
def test_002_generalRandom(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="general.random.10.30", target="response.body.integer.generalRandomBetween10and30")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "generalRandomBetween10and30":15 }
  # Replace random node to allow comparing:
  response["body"]["generalRandomBetween10and30"] = 15
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_003_generalRecvseq(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="general.recvseq", target="response.body.unsigned.generalRecvseq")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "generalRecvseq":111 }
  # Replace unknown node to allow comparing:
  response["body"]["generalRecvseq"] = 111
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_004_generalStrftime(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="general.strftime.Now it's %I:%M%p.", target="response.body.string.generalStrftime")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "generalStrftime":"Sat Jun 12 13:02:47 CEST 2021" }
  # Replace unknown node to allow comparing:
  response["body"]["generalStrftime"] = "Sat Jun 12 13:02:47 CEST 2021"
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_005_generalTimestampNs(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="general.timestamp.ns", target="response.body.unsigned.nanoseconds-timestamp")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "nanoseconds-timestamp":0 }
  # Replace unknown node to allow comparing:
  response["body"]["nanoseconds-timestamp"] = 0
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_006_inState(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="inState", target="response.body.string.in-state")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "in-state":"initial" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.xfail(strict=True, reason="rest client unable to parse non-json boolean")
def test_007_inStateToResponseBodyBoolean(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="inState", target="response.body.boolean")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  assert response["status"] == 200
  assert response["body"] == "true"

# Same for other types: h2agent could response non-json, but conftest.py canno interpret them. We will skip
# those cases, and only will test target response paths

@pytest.mark.transform
def test_008_inStateToResponseBodyBooleanPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="inState", target="response.body.boolean.inStateAsBool")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "inStateAsBool":True }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_009_valueToResponseBodyFloatPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="value.3.14", target="response.body.float.transferredValue")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "transferredValue":3.14 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_010_valueToResponseBodyIntegerPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="value.3", target="response.body.integer.transferredValue")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "transferredValue":3 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_011_objectToResponseBodyObjectPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="request.body.node1", target="response.body.object.targetForRequestNode1")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "targetForRequestNode1": { "node2":"value-of-node1-node2" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_012_requestToResponse(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="request.body", target="response.body.object")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "node1": { "node2":"value-of-node1-node2" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_013_valueToResponseBodyStringPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="value.some text", target="response.body.string.transferredValue")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "transferredValue":"some text" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_014_valueToResponseBodyUnsignedPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="value.111", target="response.body.unsigned.transferredValue")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "transferredValue":111 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_015_objectPathToResponsePath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="request.body.node1.node2", target="response.body.string.request.node1.node2")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "request":{ "node1": { "node2": "value-of-node1-node2" } } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_016_requestHeader(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="request.header.test-id", target="response.body.object.request-header-test-id")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody, { "test-type":"development", "test-id":"general" })
  responseBodyRef = { "foo":"bar-1", "request-header-test-id":"general" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.xfail(strict=True, reason="to be investigated: standalone it works")
def test_017_requestUriParamToResponseBodyStringPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter_queryParams.json.in").format(source="request.uri.param.name", target="response.body.string.parameter-name")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1?name=test")
  responseBodyRef = { 'foo': 'bar-1', "parameter-name":"test" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.xfail(strict=True, reason="to be investigated: standalone it works")
def test_018_requestUriPathToResponseBodyStringPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter_queryParams.json.in").format(source="request.uri.path", target="response.body.string.requestUriPath")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1?name=test")
  responseBodyRef = {'foo': 'bar-1', 'requestUriPath': '/app/v1/foo/bar/1'}
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_019_recvseqThroughVariableToResponseBodyUnsignedPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.intermediateVar.json.in")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["generalRecvseqFromVarAux"]
  responseBodyRef = { 'foo': 'bar-1', "generalRecvseqFromVarAux":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_020_valueToResponseBodyStringPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="value.This is a test", target="response.body.string.value")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "value":"This is a test" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_021_emptyValueToResponseBodyStringPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="value.", target="response.body.string.value")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "value":"" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_022_valueToResponseBodyJsonStringPath(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_no_filter.json.in").format(source="value.[{\\\"id\\\":\\\"2000\\\"},{\\\"id\\\":\\\"2001\\\"}]", target="response.body.jsonstring.array")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  requestBody = resources("request.json")
  response = h2ac_traffic.post("/app/v1/foo/bar/1", requestBody)
  responseBodyRef = { "foo":"bar-1", "array": [ { "id": "2000" }, { "id": "2001" } ] }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_023_virtualOutStateToSimulateRealDeletion(resources, h2ac_admin, h2ac_traffic):

  # Provisions
  requestBody = resources("server-provision_OK.json.in").format(id="13")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  requestBody = resources("server-provision_transform_no_filter_delete13.json")
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  requestBody = resources("server-provision_transform_no_filter_get13afterDeletion.json")
  response = h2ac_admin.post("/provision/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  # Firstly, exists:
  response = h2ac_traffic.get("/app/v1/foo/bar/13")
  responseBodyRef = { 'foo': 'bar-13' }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Now remove:
  response = h2ac_traffic.delete("/app/v1/foo/bar/13")
  assert response["status"] == 200

  # Now try to get, but get 404:
  response = h2ac_traffic.get("/app/v1/foo/bar/13")
  assert response["status"] == 404

