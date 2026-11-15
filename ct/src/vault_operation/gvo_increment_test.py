import pytest
import threading
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_VAULT_URI, RestClient, H2AGENT_ENDPOINT__traffic


# Vault increment (fetch-and-add): atomic under concurrent requests, the new value
# is the source; kept in a var with the same name it is the message's ticket.

URI = "/app/v1/vault-increment/attempt"
PARALLEL_URI = "/app/v1/vault-increment/parallel"


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision(h2ac_admin):

  provisions = [
    {"requestMethod": "POST", "requestUri": URI, "responseCode": 200, "responseBody": {},
     "responseHeaders": {"content-type": "application/json"},
     "transform": [
       {"from": {"source": "vault", "key": "attempts", "increment": 1}, "to": {"target": "var", "name": "attempts"}},
       {"from": {"source": "var", "name": "attempts"}, "to": {"target": "responseBody", "path": "/attempt"}},
       {"from": {"source": "math", "expression": "@{attempts} >= 3"}, "filter": [{"op": "cast", "type": "integer"}],
        "to": {"target": "var", "name": "third"}},
       {"from": {"source": "literal", "value": "503"}, "filter": [{"op": "conditionVar", "name": "third"}],
        "to": {"target": "responseStatusCode"}}
     ]},
    {"requestMethod": "POST", "requestUri": PARALLEL_URI, "responseCode": 200,
     "transform": [{"from": {"source": "vault", "key": "parallel", "increment": 1}, "to": {"target": "var", "name": "parallel"}}]}
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provisions)
  assert response["status"] == 201


@pytest.mark.server
def test_002_attempts(h2ac_admin, h2ac_traffic):

  for expected, code in ((1, 200), (2, 200), (3, 503), (4, 503)):
    response = h2ac_traffic.postDict(URI)
    assert response["status"] == code
    assert response["body"]["attempt"] == expected

  response = h2ac_admin.get(ADMIN_VAULT_URI + "?name=attempts")
  assert response["status"] == 200
  assert response["body"] == 4  # stored as an integer


@pytest.mark.server
def test_003_no_lost_update(h2ac_admin):

  # several connections in parallel (the agent may run several worker threads)
  threads, requests = 4, 25
  def run():
    client = RestClient(H2AGENT_ENDPOINT__traffic)
    for _ in range(requests): client.postDict(PARALLEL_URI)
    client.close()
  pool = [threading.Thread(target = run) for _ in range(threads)]
  for t in pool: t.start()
  for t in pool: t.join()

  response = h2ac_admin.get(ADMIN_VAULT_URI + "?name=parallel")
  assert response["status"] == 200
  assert response["body"] == threads * requests
