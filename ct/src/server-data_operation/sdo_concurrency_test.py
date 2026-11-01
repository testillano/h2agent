import pytest
from concurrent.futures import ThreadPoolExecutor
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI


@pytest.mark.admin
def test_001_concurrent_receptions_with_history_are_all_stored(admin_cleanup, h2ac_admin, h2ac_traffic):
  # Concurrent receptions of the same (new) key with data storage and history: every
  # event must be stored, and the agent must stay alive (it used to share the last
  # loaded event among worker threads without synchronization).
  admin_cleanup()
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, {"requestMethod": "POST", "requestUri": "/app/v1/concurrent", "responseCode": 201})
  assert response["status"] == 201

  total = 400
  with ThreadPoolExecutor(max_workers=16) as pool:
    statuses = list(pool.map(lambda i: h2ac_traffic.postDict("/app/v1/concurrent", {"i": i})["status"], range(total)))
  assert statuses.count(201) == total

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/concurrent")
  assert response["status"] == 200
  assert len(response["body"]["events"]) == total
