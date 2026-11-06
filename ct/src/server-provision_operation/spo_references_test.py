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


TYPED_PROVISION = {
  "requestMethod": "GET",
  "requestUri": "/app/v1/typed",
  "responseCode": "#{integer:typedCodes/ok}",
  "responseHeaders": {"content-type": "application/json"},
  "responseBody": {"book": "#{object:typedBlock}", "tags": "#{object:typedBlock/tags}",
                   "title": "#{typedBlock/title}", "asText": "#{typedCodes}"}
}


@pytest.mark.admin
def test_003_i_want_typed_references_keeping_or_converting_the_json_type(admin_cleanup, h2ac_admin, h2ac_traffic):

  admin_cleanup()
  block = {"title": "Human Action", "author": "Ludwig von Mises", "tags": ["economics", "praxeology"]}
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"typedBlock": block, "typedCodes": {"ok": "200"}})
  assert response["status"] == 201

  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, TYPED_PROVISION)
  assert response["status"] == 201
  assert "warning" not in response["body"]

  # The provision holds the typed values (consumed at load):
  expected = {"book": block, "tags": ["economics", "praxeology"], "title": "Human Action", "asText": '{"ok":"200"}'}
  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  provision = response["body"][0]
  assert provision["responseCode"] == 200          # text "200" cast to integer
  assert provision["responseBody"] == expected      # no prefix: compact json text

  response = h2ac_traffic.get("/app/v1/typed")
  assert response["status"] == 200
  assert response["body"] == expected


@pytest.mark.admin
def test_004_i_want_invalid_typed_references_kept_literally_with_a_warning(admin_cleanup, h2ac_admin):

  admin_cleanup()
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"typedCodes": {"ok": 200}, "__app_barrier:1:x": "done"})
  assert response["status"] == 201

  body = {"missing": "#{object:typedBlock}", "notWhole": "x#{object:typedCodes}", "notCastable": "#{integer:typedCodes}",
          "barrier": "#{__app_barrier:1:x}"}
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, dict(TYPED_PROVISION, responseCode=200, responseBody=body))
  assert response["status"] == 201
  warning = response["body"]["warning"]
  assert "unresolved provision references kept literally (missing vault entries at provision time): #{object:typedBlock}" in warning
  assert "typed provision references kept literally (they must be the whole string value): #{object:typedCodes}" in warning
  assert "typed provision references kept literally (vault value not convertible to the cast type): #{integer:typedCodes}" in warning

  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  assert response["body"][0]["responseBody"] == dict(body, barrier="done")  # a key with ':' is not a cast prefix

  # A typed value of the wrong type for the provision schema is rejected:
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, dict(TYPED_PROVISION, requestUri="#{object:typedCodes}"))
  assert response["status"] == 400


PRESET_PROVISION = {
  "requestMethod": "GET",
  "requestUri": "/app/v1/preset",
  "responseCode": 200,
  "responseHeaders": {"content-type": "application/json"},
  "responseBody": {"#preset": "#{object:presetSessions/default}", "supi": "imsi-214010000000001",
                   "qos": {"qci": 5}, "version": None, "plain": {"#preset": "hello"}}
}


@pytest.mark.admin
def test_005_i_want_objects_built_from_vault_presets_and_own_keys(admin_cleanup, h2ac_admin, h2ac_traffic):

  admin_cleanup()
  base = {"dnn": "internet", "qos": {"qci": 9, "arp": 8}, "rules": ["r1", "r2"], "version": 1}
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"presetSessions": {"default": base}})
  assert response["status"] == 201

  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, PRESET_PROVISION)
  assert response["status"] == 201
  assert "warning" not in response["body"]

  # Own keys win over the preset (qos merged, version removed by null); a non-reference '#preset' is data:
  expected = {"dnn": "internet", "qos": {"qci": 5, "arp": 8}, "rules": ["r1", "r2"], "supi": "imsi-214010000000001",
              "plain": {"#preset": "hello"}}
  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  assert response["body"][0]["responseBody"] == expected

  response = h2ac_traffic.get("/app/v1/preset")
  assert response["status"] == 200
  assert response["body"] == expected


@pytest.mark.admin
def test_006_i_want_invalid_presets_kept_literally_with_a_warning(admin_cleanup, h2ac_admin):

  admin_cleanup()
  response = h2ac_admin.postDict(ADMIN_VAULT_URI, {"presetSessions": {"list": [1, 2]}})
  assert response["status"] == 201

  body = {"missing": {"#preset": "#{object:presetSessions/default}", "a": 1},
          "notObject": {"#preset": "#{object:presetSessions/list}", "a": 1}}
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, dict(PRESET_PROVISION, responseBody=body))
  assert response["status"] == 201
  warning = response["body"]["warning"]
  assert "unresolved provision references kept literally (missing vault entries at provision time): #{object:presetSessions/default}" in warning
  assert "provision presets kept literally (each piece must be a #{object:} reference to an object): #{object:presetSessions/list}" in warning

  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)
  assert response["body"][0]["responseBody"] == body  # nothing applied
