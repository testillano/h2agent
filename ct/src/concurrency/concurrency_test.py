import pytest
import json
import concurrent.futures
from conftest import ADMIN_VAULT_URI, ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):
  admin_cleanup()


@pytest.mark.admin
def test_001_concurrent_vault_writes(h2ac_admin):
  """Multiple threads writing different vault keys simultaneously"""
  N = 20

  def write_key(i):
    payload = {f"key{i}": {"index": i, "data": f"value{i}"}}
    return h2ac_admin.postDict(ADMIN_VAULT_URI, payload)

  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as pool:
    futures = [pool.submit(write_key, i) for i in range(N)]
    results = [f.result() for f in futures]

  for r in results:
    assert r["status"] == 201

  # Verify all keys present and correct
  response = h2ac_admin.get(ADMIN_VAULT_URI)
  assert response["status"] == 200
  body = response["body"]
  for i in range(N):
    assert body[f"key{i}"] == {"index": i, "data": f"value{i}"}


@pytest.mark.admin
def test_002_concurrent_vault_read_write(h2ac_admin):
  """Readers and writers hitting vault simultaneously"""
  # Seed a key
  h2ac_admin.postDict(ADMIN_VAULT_URI, {"shared": "initial"})

  errors = []

  def writer(i):
    h2ac_admin.postDict(ADMIN_VAULT_URI, {"shared": f"v{i}"})

  def reader(_):
    r = h2ac_admin.get(ADMIN_VAULT_URI + "?name=shared")
    if r["status"] != 200:
      errors.append(f"unexpected status {r['status']}")

  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as pool:
    futs = []
    for i in range(15):
      futs.append(pool.submit(writer, i))
      futs.append(pool.submit(reader, i))
    for f in futs:
      f.result()

  assert len(errors) == 0


@pytest.mark.admin
def test_003_concurrent_traffic(h2ac_admin, h2ac_traffic, admin_server_provision, admin_cleanup):
  """Multiple traffic requests hitting the same provision concurrently"""
  admin_cleanup()

  provision = {
    "requestMethod": "GET",
    "requestUri": "/app/v1/concurrent-test",
    "responseCode": 200,
    "responseBody": {"result": "ok"},
    "transform": [
      {"source": "recvseq", "target": "response.body.json.unsigned./seq"}
    ]
  }
  admin_server_provision(provision)

  N = 30
  results = []

  def send_request(_):
    return h2ac_traffic.get("/app/v1/concurrent-test")

  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as pool:
    futures = [pool.submit(send_request, i) for i in range(N)]
    results = [f.result() for f in futures]

  # All should succeed
  for r in results:
    assert r["status"] == 200

  # All seqs should be unique
  seqs = [r["body"]["seq"] for r in results]
  assert len(set(seqs)) == N
