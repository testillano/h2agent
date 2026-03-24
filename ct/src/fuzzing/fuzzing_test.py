import pytest
import json
from conftest import ADMIN_VAULT_URI, ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_MATCHING_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):
  admin_cleanup()


@pytest.mark.admin
def test_001_invalid_json_body(h2ac_admin):
  """POST with malformed JSON should return 400"""
  response = h2ac_admin.post(ADMIN_SERVER_PROVISION_URI, "{ not json }")
  assert response["status"] == 400


@pytest.mark.admin
def test_002_vault_dots_in_key(h2ac_admin):
  """Vault keys with dots should be rejected by schema"""
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"key.with.dots": "value"})
  assert response["status"] == 400


@pytest.mark.admin
def test_003_provision_missing_required_fields(h2ac_admin):
  """Provision without requestMethod should fail schema validation"""
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, {
    "requestUri": "/test",
    "responseCode": 200
  })
  assert response["status"] == 400


@pytest.mark.admin
def test_004_provision_invalid_source(h2ac_admin):
  """Provision with unknown source prefix should fail"""
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, {
    "requestMethod": "GET",
    "requestUri": "/test",
    "responseCode": 200,
    "transform": [{"source": "unknown.prefix", "target": "response.body.string"}]
  })
  assert response["status"] == 400


@pytest.mark.admin
def test_005_provision_invalid_target(h2ac_admin):
  """Provision with unknown target prefix should fail"""
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, {
    "requestMethod": "GET",
    "requestUri": "/test",
    "responseCode": 200,
    "transform": [{"source": "value.hello", "target": "unknown.prefix"}]
  })
  assert response["status"] == 400


@pytest.mark.admin
def test_006_provision_unicode_value(h2ac_admin, h2ac_traffic, admin_server_provision, admin_cleanup):
  """Unicode in provision values should work"""
  admin_cleanup()
  provision = {
    "requestMethod": "GET",
    "requestUri": "/app/v1/unicode-test",
    "responseCode": 200,
    "transform": [
      {"source": "value.héllo wörld 日本語", "target": "response.body.string"}
    ]
  }
  admin_server_provision(provision)
  response = h2ac_traffic.get("/app/v1/unicode-test")
  assert response["status"] == 200
  assert response["body"] == "héllo wörld 日本語"


@pytest.mark.admin
def test_007_vault_large_object(h2ac_admin):
  """Large JSON object in vault should be stored and retrieved"""
  large = {f"field_{i}": f"value_{i}" * 10 for i in range(100)}
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"big": large})
  assert response["status"] == 201

  response = h2ac_admin.get(ADMIN_VAULT_URI + "?name=big")
  assert response["status"] == 200
  assert response["body"] == large


@pytest.mark.admin
def test_008_matching_invalid_algorithm(h2ac_admin):
  """Invalid matching algorithm should be rejected"""
  response = h2ac_admin.postDict(ADMIN_SERVER_MATCHING_URI, {"algorithm": "InvalidAlgo"})
  assert response["status"] == 400
