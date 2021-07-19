import pytest
import json
from conftest import ADMIN_PROVISION_URI


@pytest.mark.transform
@pytest.mark.filter
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.transform
@pytest.mark.filter
def test_001_append(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.Append.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-appended":"/app/v1/foo/bar/1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_002_prepend(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.Prepend.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-prepended":"prefix-/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_003_appendVar(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.AppendVar.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-appended":"/app/v1/foo/bar/1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_004_prependVar(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.PrependVar.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-prepended":"prefix-/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_005_multiply(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.Multiply.json")
  #admin_provision("transform/filter_test/provision.Multiply.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["recvseqMultipliedBy2"]
  responseBodyRef = { "foo":"bar-1", "recvseqMultipliedBy2":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_006_sum(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.Sum.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["recvseqAdding100"]
  responseBodyRef = { "foo":"bar-1", "recvseqAdding100":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_007_regexcapture(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.RegexCapture.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureBarIdFromURI":"1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_008_regexcapturemultiple(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.RegexCapture-Multiple.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureFooFromURI":"foo", "captureBarIdFromURI":"1", "variableAsIs":"/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_009_regexreplace(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.RegexReplace.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureBarIdFromURIAndAppendSuffix":"1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

@pytest.mark.transform
@pytest.mark.filter
def test_010_conditionVariable(admin_provision, h2ac_traffic):

  # Provision
  admin_provision("transform/filter_test/provision.ConditionVar.json")

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "must-be-in-response": "foo" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

