import pytest
import json
from conftest import BASIC_FOO_BAR_SERVER_PROVISION_TEMPLATE, string2dict, ADMIN_SERVER_PROVISION_URI, VALID_SERVER_PROVISIONS__RESPONSE_BODY, ADMIN_SERVER_DATA_URI
from conftest import NESTED_NODE1_NODE2_REQUEST, NESTED_VAR1_VAR2_REQUEST, TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, TRANSFORM_FOO_BAR_AND_VAR1_VAR2_SERVER_PROVISION_TEMPLATE, TRANSFORM_FOO_BAR_TWO_TRANSFERS_SERVER_PROVISION_TEMPLATE, TRANSFORM_FOO_BAR_RESPONSE_BODY_DATA_SERVER_PROVISION_TEMPLATE, TRANSFORM_FOO_BAR_COMMAND_SERVER_PROVISION_TEMPLATE


@pytest.mark.transform
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.transform
def test_001_random(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="random.10.30", target="response.body.json.integer./randomBetween10and30"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  choice = response["body"]["randomBetween10and30"]
  responseBodyRef = { "foo":"bar-1", "randomBetween10and30":choice }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_002_randomset(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="randomset.rock|paper|scissors", target="response.body.json.string./randomset"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  choice = response["body"]["randomset"]
  responseBodyRef = { "foo":"bar-1", "randomset":choice }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_003_recvseq(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="recvseq", target="response.body.json.unsigned./recvseq"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "recvseq":111 }
  # Replace unknown node to allow comparing:
  response["body"]["recvseq"] = 111
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_004_strftime(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="strftime.Now it's %I:%M%p.", target="response.body.json.string./strftime"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  value = response["body"]["strftime"]
  responseBodyRef = { "foo":"bar-1", "strftime":value }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_005_timestampNs(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="timestamp.ns", target="response.body.json.unsigned./nanoseconds-timestamp"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "nanoseconds-timestamp":0 }
  # Replace unknown node to allow comparing:
  response["body"]["nanoseconds-timestamp"] = 0
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_006_inState(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="inState", target="response.body.json.string./in-state"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "in-state":"initial" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.xfail(strict=True, reason="rest client unable to parse non-json boolean")
def test_007_inStateToResponseBodyJson_Boolean(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="inState", target="response.body.json.boolean./boolean"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  assert response["status"] == 200
  assert response["body"] == "true"

# Same for other types: h2agent could response non-json, but conftest.py canno interpret them. We will skip
# those cases, and only will test target response paths


@pytest.mark.transform
def test_008_inStateToResponseBodyJson_BooleanPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="inState", target="response.body.json.boolean./inStateAsBool"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "inStateAsBool":True }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_009_valueToResponseBodyJson_FloatPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.3.14", target="response.body.json.float./transferredValue"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "transferredValue":3.14 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_010_valueToResponseBodyJson_IntegerPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.3", target="response.body.json.integer./transferredValue"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "transferredValue":3 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_011_objectToResponseBodyJson_ObjectPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="request.body./node1", target="response.body.json.object./targetForRequestNode1"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "targetForRequestNode1": { "node2":"value-of-node1-node2" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_012_requestToResponseObject(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="request.body", target="response.body.json.object"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo": "bar-1", "node1": { "node2":"value-of-node1-node2" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_013_valueToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.some text", target="response.body.json.string./transferredValue"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "transferredValue":"some text" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_014_valueToResponseBodyJson_UnsignedPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.111", target="response.body.json.unsigned./transferredValue"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "transferredValue":111 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_015_objectPathToResponsePath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="request.body./node1/node2", target="response.body.json.string./request/node1/node2"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "request":{ "node1": { "node2": "value-of-node1-node2" } } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_016_requestHeader(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="request.header.test-id", target="response.body.json.string./request-header-test-id"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST), { "test-type":"development", "test-id":"general" })
  responseBodyRef = { "foo":"bar-1", "request-header-test-id":"general" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_017_requestUriParamToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='?name=test', source="request.uri.param.name", target="response.body.json.string./parameter-name"))

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1?name=test")
  responseBodyRef = { 'foo': 'bar-1', "parameter-name":"test" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_018_requestUriPathToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='?name=test', source="request.uri.path", target="response.body.json.string./requestUriPath"))

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1?name=test")
  responseBodyRef = {'foo': 'bar-1', 'requestUriPath': '/app/v1/foo/bar/1'}
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_019_recvseqThroughVariableToResponseBodyJson_UnsignedPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("no_filter_test.IntermediateVar.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["recvseqFromVarAux"]
  responseBodyRef = { 'foo': 'bar-1', "recvseqFromVarAux":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_020_valueToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.This is a test", target="response.body.json.string./value"))

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "value":"This is a test" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_021_emptyValueToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.", target="response.body.json.string./value"))

  # Traffic
  response = h2ac_traffic.post("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "value":"" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_022_valueToResponseBodyJson_JsonStringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.[{\\\"id\\\":\\\"2000\\\"},{\\\"id\\\":\\\"2001\\\"}]", target="response.body.json.jsonstring./array"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "array": [ { "id": "2000" }, { "id": "2001" } ] }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_023_virtualOutStateToSimulateRealDeletion(admin_server_provision, h2ac_traffic):

  # Provisions
  admin_server_provision("no_filter_test.VirtualOutState.provision.json", responseBodyRef = VALID_SERVER_PROVISIONS__RESPONSE_BODY)

  # Firstly, exists:
  response = h2ac_traffic.get("/app/v1/foo/bar/13")
  h2ac_traffic.assert_response__status_body_headers(response, 200, { 'foo': 'bar-13' })

  # Now remove:
  response = h2ac_traffic.delete("/app/v1/foo/bar/13")
  assert response["status"] == 200

  # Now missing (404):
  response = h2ac_traffic.get("/app/v1/foo/bar/13")
  assert response["status"] == 404


@pytest.mark.transform
def test_024_serverEventToResponseBodyPath(admin_cleanup, admin_server_provision, h2ac_traffic):

  # Cleanup
  admin_cleanup()

  # Provision
  admin_server_provision("no_filter_test.ServerEvent.provision.json")

  # Traffic

  # First time, there is nothing (no event number 0 exists), so, responseBody won't add event information:
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1?name=test&val=2", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Second time, we will have the previous traffic event as reference (number 1), and responseBody will add
  # the request body which was received. We will send now another different request body to make the test more robust:
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1?name=test&val=2", { "anotherRequest": "foo" })
  responseBodyRef["firstRequestBody"] = string2dict(NESTED_NODE1_NODE2_REQUEST)
  responseBodyRef["foo"] = "bar-1"
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_025_serverEventUriEncodedToResponseBodyPath(admin_cleanup, admin_server_provision, h2ac_traffic):

  # Cleanup
  admin_cleanup()

  # Provision
  admin_server_provision("no_filter_test.ServerEventUriEncoded.provision.json")

  # Traffic

  # First time, there is nothing (no event number 0 exists), so, responseBody won't add event information:
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1?name=test&val=2", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Second time, we will have the previous traffic event as reference (number 1), and responseBody will add
  # the request body which was received. We will send now another different request body to make the test more robust:
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1?name=test&val=2", { "anotherRequest": "foo" })
  responseBodyRef["firstRequestBody"] = string2dict(NESTED_NODE1_NODE2_REQUEST)
  responseBodyRef["foo"] = "bar-1"
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_026_serverEventPurge(admin_cleanup, admin_server_provision, h2ac_traffic, h2ac_admin):

  # Cleanup
  admin_cleanup()

  # Provision
  admin_server_provision("no_filter_test.ServerEventPurge.provision.json", VALID_SERVER_PROVISIONS__RESPONSE_BODY)

  # Traffic

  # Send 3 GET requests to /foo/bar (does not matter the fact they are not provisioned, because it will be part of server data storage):
  response = h2ac_traffic.get("/app/v1/foo/bar")
  response = h2ac_traffic.get("/app/v1/foo/bar")
  response = h2ac_traffic.get("/app/v1/foo/bar")

  # Check we have 3 GETs:
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar&eventNumber=3")
  assert(response["status"] == 200)
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar&eventNumber=4")
  assert(response["status"] == 204)

  # Force traffic to POST provision which removes last GET:
  response = h2ac_traffic.postDict("/app/v1/foo/bar/removeLastGET", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check that only 2 GETs are remaining:
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar&eventNumber=2")
  assert(response["status"] == 200)
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar&eventNumber=3")
  assert(response["status"] == 204)

  # Force traffic to POST provision which removes all GETs (2 alive):
  response = h2ac_traffic.postDict("/app/v1/foo/bar/removeAllGET", string2dict(NESTED_NODE1_NODE2_REQUEST))
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

  # Check that only no GETs are remaining:
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar")
  assert(response["status"] == 204)


@pytest.mark.transform
def test_027_valueWithVariables(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("no_filter_test.ReplaceValueVariables.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { 'foo': 'bar-1', "whoAreYou": "My name is Phil Collins" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_028_eraseResponseBody(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("no_filter_test.EraseResponseBody.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = ""
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_029_eraseResponseBodyNode(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("no_filter_test.EraseResponseBodyNode.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { 'foo': 'bar-1', "item": { "color": "white", "size": 50 } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_030_eraseResponseBodyNodeValue(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("no_filter_test.EraseResponseBodyNodeValue.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { 'foo': 'bar-1', "item": { "color": "white", "size": 50, "planet": "" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_031_responseObjectToResponseBodyJson_ObjectPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="response.body./foo", target="response.body.json.object./fooAgain"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "fooAgain": "bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_032_responseToResponseBodyJson_ObjectPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="response.body", target="response.body.json.object./responseAgain"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "responseAgain": { "foo":"bar-1" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_033_replaceVariablesAtValueAndTransferToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.@{var1}-@{var2}", target="response.body.json.string./result"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "result":"var1value-var2value" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_034_replaceVariablesAtStrftimeAndTransferToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="strftime.Now it's %I:%M%p and var1 is @{var1}.", target="response.body.json.string./result"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  value = response["body"]["result"]
  responseBodyRef = { "foo":"bar-1", "result":value }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)
  pos = value.find('and var1 is var1value.')
  assert(pos != -1)


@pytest.mark.transform
def test_035_replaceVariablesAtRandomsetAndTransferToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="randomset.@{var1}|@{var2}", target="response.body.json.string./result"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  choice = response["body"]["result"]
  responseBodyRef = { "foo":"bar-1", "result":choice }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)
  assert(choice == "var1value" or choice == "var2value")


@pytest.mark.transform
def test_036_replaceVariablesAtRequestBodyPathAndTransferToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="request.body./@{var1}/@{var2}", target="response.body.json.string./result"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_VAR1_VAR2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "result": "value-of-var1value-var2value" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_037_replaceVariablesAtResponseBodyPathAndTransferToResponseBodyJson_StringPath(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_AND_VAR1_VAR2_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="response.body./@{var1}/@{var2}", target="response.body.json.string./result"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "var1value": { "var2value": "value-of-var1value-var2value" }, "result": "value-of-var1value-var2value" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_038_transferFixedValueToResponseBodyPathWithReplacedVariablesAsString(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.hello", target="response.body.json.string./@{var1}/@{var2}"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "var1value": { "var2value": "hello" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_039_transferFixedValueToResponseBodyPathWithReplacedVariablesAsInteger(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.-1", target="response.body.json.integer./@{var1}/@{var2}"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "var1value": { "var2value": -1 } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_040_transferFixedValueToResponseBodyPathWithReplacedVariablesAsUnsigned(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.1", target="response.body.json.unsigned./@{var1}/@{var2}"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "var1value": { "var2value": 1 } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_041_transferFixedValueToResponseBodyPathWithReplacedVariablesAsFloat(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.3.14", target="response.body.json.float./@{var1}/@{var2}"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "var1value": { "var2value": 3.14 } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_042_transferFixedValueToResponseBodyPathWithReplacedVariablesAsBoolean(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.true", target="response.body.json.boolean./@{var1}/@{var2}"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "var1value": { "var2value": True } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_043_transferFixedValueToResponseBodyPathWithReplacedVariablesAsObject(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.hello", target="response.body.json.object./@{var1}/@{var2}"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "var1value": { "var2value": "hello" } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_044_transferFixedValueToResponseBodyPathWithReplacedVariablesAsJsonstring(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.[\\\"aa\\\",\\\"bb\\\"]", target="response.body.json.jsonstring./@{var1}/@{var2}"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "var1value": { "var2value": [ "aa", "bb" ] } }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_045_transferFixedValueToHeaderNameWithReplacedVariables(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.header-value", target="response.header.@{var1}-@{var2}"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef, headersDict = {})
  myHeader = response["headers"]["var1value-var2value"][0]
  assert(myHeader.decode('utf-8') == "header-value")


@pytest.mark.transform
def test_046_transferFixedValueToVariableNameWithReplacedVariables(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_TWO_TRANSFERS_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="value.var1valuevalue", target="var.@{var1}", source2="var.@{var1}", target2="response.body.json.string./result"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "result": "var1valuevalue" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_047_requestBodyStringToResponseObject(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="request.body", target="response.body.json.object"))

  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST), requestHeaders=None) # request body will be interpreted as string
  responseBodyRef = "{\"node1\":{\"node2\":\"value-of-node1-node2\"}}"
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_048_requestBodyStringToResponseString(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="request.body", target="response.body.json.string"))

  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST), requestHeaders=None) # request body will be interpreted as string
  responseBodyRef = "{\"node1\":{\"node2\":\"value-of-node1-node2\"}}" # although it seems a json, application/json content-type was missing,
                                                                       # so, it is interpreted like any other string.
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_049_requestBodyJsonToResponseString(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="request.body", target="response.body.string"))

  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = string2dict(NESTED_NODE1_NODE2_REQUEST) # { "foo":"bar-1" } on response template is replaced
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_050_mathCalculationToResponseInteger(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=1, queryp='', source="math.1+2+3+5+8", target="response.body.json.integer./math-calculation"))

  response = h2ac_traffic.postDict("/app/v1/foo/bar/1", string2dict(NESTED_NODE1_NODE2_REQUEST))
  responseBodyRef = { "foo":"bar-1", "math-calculation":19 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_051_valueToResponseBodyString(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_RESPONSE_BODY_DATA_SERVER_PROVISION_TEMPLATE, ct="text/html", value="hello", what="string"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar")
  h2ac_traffic.assert_response__status_body_headers(response, 200, "hello")


def test_052_valueToResponseBodyBadHexStringOddNumber(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_RESPONSE_BODY_DATA_SERVER_PROVISION_TEMPLATE, ct="application/octet-stream", value="value.0xc0a", what="hexstring"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar")
  h2ac_traffic.assert_response__status_body_headers(response, 200, "")


# TODO: cannot check binary responses with hyper rest client
#def test_053_valueToResponseBodyBadHexString(admin_server_provision, h2ac_traffic):
#
#  # Provision
#  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_RESPONSE_BODY_DATA_SERVER_PROVISION_TEMPLATE, ct="application/octet-stream", value="value.0xc0a8", what="hexstring"))
#
#  # Traffic
#  response = h2ac_traffic.postDict("/app/v1/foo/bar")
#  h2ac_traffic.assert_response__status_body_headers(response, 200, "")


def test_053_commandExecutionToResponseBodyString(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(TRANSFORM_FOO_BAR_COMMAND_SERVER_PROVISION_TEMPLATE, ct="text/html", command="echo -n foo"))

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar")
  responseBodyRef = { "output":"foo", "rc":0 }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
def test_054_breakTransformations(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("no_filter_test.Break.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  h2ac_traffic.assert_response__status_body_headers(response, 200, "response-body-string")

