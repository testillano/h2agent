import pytest
import os
import time
from conftest import (
    ADMIN_DATASET_URI, ADMIN_VAULT_URI, ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI, ADMIN_CLIENT_PROVISION_URI,
    VALID_CLIENT_ENDPOINT__RESPONSE_BODY, string2dict, CLIENT_ENDPOINT_TEMPLATE
)

# Datasets: tables loaded from files the agent reads, rows read by the 'dataset' source
# (row modulo the rows). The files are written in the agent container by a provision
# with a command source (the CT pod has no access to the agent filesystem).

H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))
LOOPBACK_ENDPOINT = string2dict(CLIENT_ENDPOINT_TEMPLATE, id="loopback", port=H2AGENT_TRAFFIC_PORT)
LOOPBACK_ENDPOINT["host"] = H2AGENT_HOST

CREW = "/tmp/ct-crew.txt"
CARGO = "/tmp/ct-cargo.txt"
BROKEN = "/tmp/ct-broken.txt"
FILES = {
  CREW: ["# crew of the ship", "", "name    role       deck", "alice   captain    1", "bob     engineer   3", "carol   cook       2"],
  CARGO: ["item           note", "cargo%20box    100%25", "crate          fragile"],
  BROKEN: ["name role", "alice captain", "bob"],
}
NAMES = ["alice", "bob", "carol"]


def write_files(h2ac_admin, h2ac_traffic):
  # printf '%s\n' prints its arguments as they are (no format in the values)
  command = " ; ".join("printf '%s\\n' " + " ".join("'" + line + "'" for line in lines) + " > " + path for path, lines in FILES.items())
  provision = {"requestMethod": "POST", "requestUri": "/app/v1/dataset/write", "responseCode": 200,
               "transform": [{"from": {"source": "command", "command": command}, "to": {"target": "var", "name": "output"}}]}
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provision)["status"] == 201
  assert h2ac_traffic.postDict("/app/v1/dataset/write")["status"] == 200


@pytest.mark.admin
def test_001_load_and_summary(admin_cleanup, h2ac_admin, h2ac_traffic):
  admin_cleanup()
  write_files(h2ac_admin, h2ac_traffic)
  assert h2ac_admin.get(ADMIN_DATASET_URI)["status"] == 204

  response = h2ac_admin.put(ADMIN_DATASET_URI + "?file=" + CREW + "&id=crew")
  assert response["status"] == 200
  assert response["body"] == {"id": "crew", "columns": ["name", "role", "deck"], "rows": 3}

  response = h2ac_admin.put(ADMIN_DATASET_URI + "?file=" + CARGO + "&decode=true")  # id: basename without extension
  assert response["status"] == 200
  assert response["body"] == {"id": "ct-cargo", "columns": ["item", "note"], "rows": 2}

  response = h2ac_admin.get(ADMIN_DATASET_URI)
  assert response["status"] == 200
  assert response["body"] == [{"id": "crew", "columns": ["name", "role", "deck"], "rows": 3},
                              {"id": "ct-cargo", "columns": ["item", "note"], "rows": 2}]
  assert h2ac_admin.get(ADMIN_DATASET_URI + "?id=crew")["body"]["rows"] == 3
  assert h2ac_admin.get(ADMIN_DATASET_URI + "?id=ghost")["status"] == 204


@pytest.mark.admin
def test_002_load_errors(h2ac_admin):
  response = h2ac_admin.put(ADMIN_DATASET_URI + "?file=" + BROKEN + "&id=crew")
  assert response["status"] == 400
  assert response["body"]["response"] == "dataset operation; file '" + BROKEN + "', line 3: expected 2 fields, found 1"
  assert h2ac_admin.get(ADMIN_DATASET_URI + "?id=crew")["body"]["rows"] == 3  # the former one is kept

  assert h2ac_admin.put(ADMIN_DATASET_URI + "?file=/tmp/ct-missing.txt")["status"] == 404
  assert h2ac_admin.put(ADMIN_DATASET_URI + "?id=crew")["status"] == 400                       # no file
  assert h2ac_admin.put(ADMIN_DATASET_URI + "?file=" + CREW + "&decode=yes")["status"] == 400  # decode true/false
  assert h2ac_admin.put(ADMIN_DATASET_URI + "?file=" + CREW + "&id=bad%20id")["status"] == 400  # id characters


@pytest.mark.admin
def test_003_isolated_from_the_vault(h2ac_admin):
  response = h2ac_admin.get(ADMIN_VAULT_URI)
  assert "crew" not in (response["body"] or {})
  h2ac_admin.delete(ADMIN_VAULT_URI)
  assert h2ac_admin.get(ADMIN_DATASET_URI + "?id=crew")["status"] == 200


@pytest.mark.server
def test_004_server_rows(h2ac_admin, h2ac_traffic):
  provisions = [
    {"requestMethod": "GET", "requestUri": "/app/v1/dataset/member", "responseCode": 200, "responseBody": {},
     "responseHeaders": {"content-type": "application/json"},
     "transform": [
       {"from": {"source": "recvseq"}, "to": {"target": "var", "name": "recvseq"}},
       {"from": {"source": "dataset", "name": "crew", "row": "@{recvseq}"}, "to": {"target": "var", "name": "member"}},
       {"from": {"source": "var", "name": "recvseq"}, "filter": [{"op": "cast", "type": "integer"}], "to": {"target": "responseBody", "path": "/recvseq"}},
       {"from": {"source": "var", "name": "member"}, "to": {"target": "responseBody", "path": "/member"}}
     ]},
    {"requestMethod": "GET", "requestUri": "/app/v1/dataset/last-cargo", "responseCode": 200, "responseBody": {},
     "responseHeaders": {"content-type": "application/json"},
     "transform": [
       {"from": {"source": "dataset", "name": "ct-cargo", "row": -2}, "to": {"target": "responseBody", "path": "/first"}},
       {"from": {"source": "dataset", "name": "ghost", "row": 0}, "to": {"target": "responseBody", "path": "/ghost"}},
       {"from": {"source": "dataset", "name": "crew", "row": "@{nothing}"}, "to": {"target": "responseBody", "path": "/badRow"}}
     ]}
  ]
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provisions)["status"] == 201

  for _ in range(4):
    response = h2ac_traffic.get("/app/v1/dataset/member")
    assert response["status"] == 200
    body = response["body"]
    expected = NAMES[body["recvseq"] % 3]
    assert body["member"]["name"] == expected
    assert set(body["member"].keys()) == {"name", "role", "deck"}

  # a negative row counts from the end (decoded values); unknown dataset or a non integer row: rule skipped
  response = h2ac_traffic.get("/app/v1/dataset/last-cargo")
  assert response["status"] == 200
  assert response["body"] == {"first": {"item": "cargo box", "note": "100%"}}


@pytest.mark.client
def test_005_client_rows_repeat(h2ac_admin, admin_client_endpoint):
  admin_client_endpoint(LOOPBACK_ENDPOINT, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)
  assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, {"requestMethod": "POST", "requestUri": "/app/v1/dataset/board", "responseCode": 200})["status"] == 201
  client = {
    "id": "boarding", "endpoint": "loopback", "requestMethod": "POST", "requestUri": "/app/v1/dataset/board",
    "requestHeaders": {"content-type": "application/json"},
    "transform": [
      {"from": {"source": "dataset", "name": "crew", "row": "@{sequence}"}, "to": {"target": "var", "name": "member"}},
      {"from": {"source": "var", "name": "sequence"}, "filter": [{"op": "cast", "type": "integer"}], "to": {"target": "requestBody", "path": "/seq"}},
      {"from": {"source": "var", "name": "member", "path": "/name"}, "to": {"target": "requestBody", "path": "/name"}}
    ]
  }
  assert h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, client)["status"] == 201

  # 7 requests over 3 rows: the rows repeat
  assert h2ac_admin.post(ADMIN_CLIENT_PROVISION_URI + "/boarding/trigger?sequenceBegin=0&sequenceEnd=6&cps=50")["status"] == 202

  def sent():
    response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/dataset/board")
    if response["status"] != 200: return []
    return [(e["requestBody"]["seq"], e["requestBody"]["name"]) for e in response["body"]["events"]]

  deadline = time.time() + 5
  while time.time() < deadline and len(sent()) < 7: time.sleep(0.1)
  assert sorted(sent()) == [(s, NAMES[s % 3]) for s in range(7)]


@pytest.mark.admin
def test_006_delete(h2ac_admin):
  assert h2ac_admin.delete(ADMIN_DATASET_URI + "?id=crew")["status"] == 200
  assert h2ac_admin.delete(ADMIN_DATASET_URI + "?id=crew")["status"] == 204
  assert h2ac_admin.get(ADMIN_DATASET_URI + "?id=crew")["status"] == 204
  assert h2ac_admin.delete(ADMIN_DATASET_URI)["status"] == 200  # ct-cargo
  assert h2ac_admin.delete(ADMIN_DATASET_URI)["status"] == 204
  assert h2ac_admin.get(ADMIN_DATASET_URI)["status"] == 204
