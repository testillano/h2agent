import pytest
import json


@pytest.mark.transform
@pytest.mark.filter
def test_001_cleanup_provisions(resources, h2ac_admin):
  response = h2ac_admin.delete("/admin/v1/server-provision")


@pytest.mark.transform
@pytest.mark.filter
def test_002_append(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_Append.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-appended":"/app/v1/foo/bar/1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_003_prepend(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_Prepend.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-prepended":"prefix-/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_004_appendVar(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_AppendVar.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-appended":"/app/v1/foo/bar/1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_005_prependVar(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_PrependVar.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "uri-prepended":"prefix-/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_006_multiply(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_Multiply.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["recvseqMultipliedBy2"]
  responseBodyRef = { "foo":"bar-1", "recvseqMultipliedBy2":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_007_sum(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_Sum.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  val = response["body"]["recvseqAdding100"]
  responseBodyRef = { "foo":"bar-1", "recvseqAdding100":val }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_008_regexcapture(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_RegexCapture.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureBarIdFromURI":"1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_009_regexcapturemultiple(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_RegexCaptureMultiple.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureFooFromURI":"foo", "captureBarIdFromURI":"1", "variableAsIs":"/app/v1/foo/bar/1" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.transform
@pytest.mark.filter
def test_010_regexreplace(resources, h2ac_admin, h2ac_traffic):

  # Provision
  requestBody = resources("server-provision_transform_filter_RegexReplace.json")
  responseBodyRef = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
  response = h2ac_admin.post("/admin/v1/server-provision", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Traffic
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = { "foo":"bar-1", "captureBarIdFromURIAndAppendSuffix":"1-suffix" }
  h2ac_traffic.assert_response__status_body_headers(response, 200, responseBodyRef)

