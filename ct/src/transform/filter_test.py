import pytest
import json
from conftest import ADMIN_SERVER_PROVISION_URI, NLOHMANN_EXAMPLE_REQUEST, string2dict


@pytest.mark.transform
@pytest.mark.filter
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.transform
@pytest.mark.filter
def test_001_append(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.Append.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-appended":"/app/v1/foo/bar/1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_002_prepend(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.Prepend.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-prepended":"prefix-/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_003_appendWithVariable(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.AppendWithVariable.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-appended":"/app/v1/foo/bar/1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_004_prependWithVariable(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.PrependWithVariable.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-prepended":"prefix-/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_005_multiply(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.Multiply.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["recvseqMultipliedBy2"]
  responseBodyRef = { "foo":"bar-1", "recvseqMultipliedBy2":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_006_sum(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.Sum.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["recvseqAdding100"]
  responseBodyRef = { "foo":"bar-1", "recvseqAdding100":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_007_regexcapture(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.RegexCapture.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureBarIdFromURI":"1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_008_regexcapturemultiple(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.RegexCapture-Multiple.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureFooFromURI":"foo", "captureBarIdFromURI":"1", "variableAsIs":"/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_009_regexreplace(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.RegexReplace.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureBarIdFromURIAndAppendSuffix":"1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_010_conditionVariable(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.ConditionVar.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "must-be-in-response": "foo" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_011_like(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.EqualTo.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "must-be-in-response": "hello" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_012_likeWithVariable(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.EqualToWithVariable.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "must-be-in-response": "hello" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_013_diff(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.DifferentFrom.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "must-be-in-response": "hello" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_014_diffWithVariable(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.DifferentFromWithVariable.provision.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "must-be-in-response": "hello" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_015_jsonConstraintSuccess(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.JsonConstraintSuccess.provision.json")

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar", string2dict(NLOHMANN_EXAMPLE_REQUEST))

  h2ac_traffic.assert_response__status_body_headers(response, 200, "")


@pytest.mark.transform
@pytest.mark.filter
def test_016_jsonConstraintFail(admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision("filter_test.JsonConstraintFail.provision.json")

  # Traffic
  response = h2ac_traffic.postDict("/app/v1/foo/bar", string2dict(NLOHMANN_EXAMPLE_REQUEST))

  h2ac_traffic.assert_response__status_body_headers(response, 400, "JsonConstraint FAILED: expected key 'I_WANT_THIS_KEY' is missing in validated source")

