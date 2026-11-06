import pytest
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_VAULT_URI


REFERENCED_PROVISION = {
  "requestMethod": "GET",
  "requestUri": "/app/v1/ref/#{refSuffix}",
  "responseCode": 200,
  "responseHeaders": {"content-type": "application/json"},
  "responseBody": {"version": "#{refConfig/version}"},
  "transform": [
    {"from": {"source": "requestUriPath"}, "filter": [{"op": "regexReplace", "rgx": "/app/v1/ref/#{refPattern}", "fmt": "$1"}],
     "to": {"target": "responseBody", "path": "/id"}}
  ]
}


@pytest.mark.admin
def test_001_i_want_to_resolve_provision_references_at_load(admin_cleanup, h2ac_admin, h2ac_traffic):

  admin_cleanup()
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"refSuffix": "id-42", "refPattern": "id-([0-9]+)", "refConfig": {"version": "1.2"}})
  assert response["status"] == 201

  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, REFERENCED_PROVISION)
  assert response["status"] == 201

  # The provision holds the resolved values (references are consumed at load):
  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  provision = response["body"][0]
  assert provision["requestUri"] == "/app/v1/ref/id-42"
  assert provision["responseBody"] == {"version": "1.2"}
  assert provision["transform"][0]["filter"][0]["rgx"] == "/app/v1/ref/id-([0-9]+)"

  response = h2ac_traffic.get("/app/v1/ref/id-42")
  assert response["status"] == 200
  assert response["body"] == {"version": "1.2", "id": "42"}

  # Later vault changes are not observed (references are never dynamic):
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"refConfig": {"version": "9.9"}})
  response = h2ac_traffic.get("/app/v1/ref/id-42")
  assert response["body"]["version"] == "1.2"


@pytest.mark.admin
def test_002_i_want_missing_references_kept_literally_with_a_warning(admin_cleanup, h2ac_admin):

  admin_cleanup()
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, REFERENCED_PROVISION)
  assert response["status"] == 201
  for ref in ["#{refConfig/version}", "#{refPattern}", "#{refSuffix}"]:
    assert ref in response["body"]["warning"]

  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  provision = response["body"][0]
  assert provision["requestUri"] == "/app/v1/ref/#{refSuffix}"
  assert provision["responseBody"] == {"version": "#{refConfig/version}"}


INCLUDED_PROVISION = {
  "requestMethod": "GET",
  "requestUri": "/app/v1/include",
  "responseCode": {"#include": "incCodes/ok"},
  "responseHeaders": {"content-type": "application/json"},
  "responseBody": {"book": {"#include": "incBlock"}, "tags": {"#include": "incBlock/tags"}}
}


@pytest.mark.admin
def test_003_i_want_to_include_vault_values_keeping_their_json_type(admin_cleanup, h2ac_admin, h2ac_traffic):

  admin_cleanup()
  block = {"title": "Human Action", "author": "Ludwig von Mises", "tags": ["economics", "praxeology"]}
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"incBlock": block, "incCodes": {"ok": 200}})
  assert response["status"] == 201

  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, INCLUDED_PROVISION)
  assert response["status"] == 201

  # The provision holds the included values (consumed at load):
  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  provision = response["body"][0]
  assert provision["responseCode"] == 200
  assert provision["responseBody"] == {"book": block, "tags": ["economics", "praxeology"]}

  response = h2ac_traffic.get("/app/v1/include")
  assert response["status"] == 200
  assert response["body"] == {"book": block, "tags": ["economics", "praxeology"]}


@pytest.mark.admin
def test_004_i_want_undefined_or_malformed_includes_kept_literally_with_a_warning(admin_cleanup, h2ac_admin):

  admin_cleanup()
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"incCodes": {"ok": 200}})
  assert response["status"] == 201

  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, INCLUDED_PROVISION)
  assert response["status"] == 201
  assert "reserved key '#include' ignored, undefined vault entry" in response["body"]["warning"]
  assert '{"#include":"incBlock"}' in response["body"]["warning"]

  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  assert response["body"][0]["responseBody"]["book"] == {"#include": "incBlock"}

  # Not an include (other keys): a regular object, answered as it is:
  body = {"book": {"#include": "incCodes", "extra": 1}}
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, dict(INCLUDED_PROVISION, responseBody=body))
  assert response["status"] == 201
  assert "reserved key '#include' ignored, not the only key or not a string" in response["body"]["warning"]

  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  assert response["body"][0]["responseBody"] == body
