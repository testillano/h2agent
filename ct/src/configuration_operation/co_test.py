import pytest
import json
from conftest import ADMIN_CONFIGURATION_URI, ADMIN_URI_PREFIX


@pytest.mark.admin
def test_001_i_want_to_get_general_process_configuration(h2ac_admin):

  # Check configuration
  response = h2ac_admin.get(ADMIN_CONFIGURATION_URI)
  responseBodyRef = { 'disableMetrics': False, 'lazyClientConnection': True, "longTermFilesCloseDelayUsecs": 1000000, "queueDispatcherMaxSize": -1, "shortTermFilesCloseDelayUsecs": 0, "trafficClientConnections": 1, "trafficServerMaxWorkerThreads": 1, "trafficServerWorkerThreads": 1 }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)


@pytest.mark.admin
def test_002_i_want_to_get_capabilities(h2ac_admin):

  # Diagnostics only (clients hardcode the admin API version)
  response = h2ac_admin.get(ADMIN_URI_PREFIX + "capabilities")
  assert response["status"] == 200
  assert response["body"]["apiVersion"] == "v2"
  assert response["body"]["grammar"] == "v2"
  assert "programVersion" in response["body"]


@pytest.mark.admin
def test_003_i_want_to_get_the_admin_api_schemas(h2ac_admin):

  resources = ["server-matching", "server-provision", "client-endpoint", "client-provision", "schema", "vault"]

  # List of resources:
  response = h2ac_admin.get(ADMIN_URI_PREFIX + "api-schema")
  assert response["status"] == 200
  assert response["body"] == resources

  # Each resource schema (with its $id):
  for r in resources:
    response = h2ac_admin.get(ADMIN_URI_PREFIX + "api-schema/" + r)
    assert response["status"] == 200
    assert response["body"]["$schema"] == "http://json-schema.org/draft-07/schema#"
    assert response["body"]["$id"].endswith("/admin/v2/api-schema/" + r)

  # Unknown resource:
  response = h2ac_admin.get(ADMIN_URI_PREFIX + "api-schema/unknown")
  assert response["status"] == 404


@pytest.mark.admin
def test_004_i_want_a_hint_for_the_retired_resource_schema_paths(h2ac_admin):

  for r in ["server-matching", "server-provision", "client-endpoint", "client-provision", "schema", "vault"]:
    response = h2ac_admin.get(ADMIN_URI_PREFIX + r + "/schema")
    assert response["status"] == 410
    assert ("/admin/v2/api-schema/" + r) in response["body"]["response"]
