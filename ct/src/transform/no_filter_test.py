import pytest
import json
from conftest import BASIC_FOO_BAR_PROVISION_TEMPLATE, string2dict, ADMIN_PROVISION_URI, NESTED_NODE1_NODE2_REQUEST, TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, VALID_PROVISIONS__RESPONSE_BODY


@pytest.mark.transform
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.transform
def test_001_generalRandom(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="general.random.10.30", target="response.body.integer.generalRandomBetween10and30"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "generalRandomBetween10and30":15 }
  # Replace random node to allow comparing:
  response["body"]["generalRandomBetween10and30"] = 15
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_002_generalRecvseq(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="general.recvseq", target="response.body.unsigned.generalRecvseq"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "generalRecvseq":111 }
  # Replace unknown node to allow comparing:
  response["body"]["generalRecvseq"] = 111
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_003_generalStrftime(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="general.strftime.Now it's %I:%M%p.", target="response.body.string.generalStrftime"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "generalStrftime":"Sat Jun 12 13:02:47 CEST 2021" }
  # Replace unknown node to allow comparing:
  response["body"]["generalStrftime"] = "Sat Jun 12 13:02:47 CEST 2021"
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_004_generalTimestampNs(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="general.timestamp.ns", target="response.body.unsigned.nanoseconds-timestamp"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "nanoseconds-timestamp":0 }
  # Replace unknown node to allow comparing:
  response["body"]["nanoseconds-timestamp"] = 0
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_005_inState(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="inState", target="response.body.string.in-state"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "in-state":"initial" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.xfail(strict=True, reason="rest client unable to parse non-json boolean")
def test_006_inStateToResponseBodyBoolean(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="inState", target="response.body.boolean"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  assert response["status"] == 200
  assert response["body"] == "true"

# Same for other types: h2agent could response non-json, but conftest.py canno interpret them. We will skip
# those cases, and only will test target response paths

@pytest.mark.transform
def test_007_inStateToResponseBodyBooleanPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="inState", target="response.body.boolean.inStateAsBool"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "inStateAsBool":True }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_008_valueToResponseBodyFloatPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="value.3.14", target="response.body.float.transferredValue"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "transferredValue":3.14 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_009_valueToResponseBodyIntegerPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="value.3", target="response.body.integer.transferredValue"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "transferredValue":3 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_010_objectToResponseBodyObjectPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="request.body.node1", target="response.body.object.targetForRequestNode1"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "targetForRequestNode1": { "node2":"value-of-node1-node2" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_011_requestToResponse(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="request.body", target="response.body.object"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "node1": { "node2":"value-of-node1-node2" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_012_valueToResponseBodyStringPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="value.some text", target="response.body.string.transferredValue"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "transferredValue":"some text" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_013_valueToResponseBodyUnsignedPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="value.111", target="response.body.unsigned.transferredValue"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "transferredValue":111 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_014_objectPathToResponsePath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="request.body.node1.node2", target="response.body.string.request.node1.node2"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "request":{ "node1": { "node2": "value-of-node1-node2" } } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_015_requestHeader(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="request.header.test-id", target="response.body.object.request-header-test-id"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST), { "test-type":"development", "test-id":"general" })
  responseBodyRef = { "foo":"bar-1", "request-header-test-id":"general" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_016_requestUriParamToResponseBodyStringPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='?name=test', source="request.uri.param.name", target="response.body.string.parameter-name"))

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1?name=test")
  responseBodyRef = { 'foo': 'bar-1', "parameter-name":"test" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_017_requestUriPathToResponseBodyStringPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='?name=test', source="request.uri.path", target="response.body.string.requestUriPath"))

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1?name=test")
  responseBodyRef = {'foo': 'bar-1', 'requestUriPath': '/app/v1/foo/bar/1'}
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_018_recvseqThroughVariableToResponseBodyUnsignedPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("provision.transform.no_filter_test.IntermediateVar.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["generalRecvseqFromVarAux"]
  responseBodyRef = { 'foo': 'bar-1', "generalRecvseqFromVarAux":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_019_valueToResponseBodyStringPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="value.This is a test", target="response.body.string.value"))

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "value":"This is a test" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_020_emptyValueToResponseBodyStringPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="value.", target="response.body.string.value"))

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "value":"" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_021_valueToResponseBodyJsonStringPath(admin_provision, h2ac_traffic):

  # Provision
  admin_provision(string2dict(TRANSFORM_FOO_BAR_PROVISION_TEMPLATE, id=1, queryp='', source="value.[{\\\"id\\\":\\\"2000\\\"},{\\\"id\\\":\\\"2001\\\"}]", target="response.body.jsonstring.array"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "array": [ { "id": "2000" }, { "id": "2001" } ] }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_022_virtualOutStateToSimulateRealDeletion(admin_provision, h2ac_traffic):

  # Provisions
  admin_provision("provision.transform.no_filter_test.VirtualOutState.json", responseBodyRef = VALID_PROVISIONS__RESPONSE_BODY)

  # Firstly, exists:
  response = h2ac_traffic.get("/app/v1/foo/bar/13")
  h2ac_traffic.assert_response__status_body_headers(response, 200, { 'foo': 'bar-13' })

  # Now remove:
  response = h2ac_traffic.delete("/app/v1/foo/bar/13")
  assert response["status"] == 200

  # Now missing (404):
  response = h2ac_traffic.get("/app/v1/foo/bar/13")
  assert response["status"] == 404

