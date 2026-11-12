import pytest
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI


# A received text which is not UTF-8 is stored as it is, and dumped with U+FFFD in
# place of the invalid bytes: server-data keeps every event readable (it came out
# empty) and the process stays alive.

REPLACEMENT = "\ufffd"


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision(h2ac_admin):

  provisions = [
    {"requestMethod": "POST", "requestUri": "/app/v1/invalid-utf8/text", "responseCode": 200},
    {"requestMethod": "POST", "requestUri": "/app/v1/invalid-utf8/other", "responseCode": 201}
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provisions)
  assert response["status"] == 201


@pytest.mark.server
def test_002_server_data_keeps_every_event(h2ac_admin, h2ac_traffic):

  response = h2ac_traffic.postDict("/app/v1/invalid-utf8/other", {"ok": 1})
  assert response["status"] == 201
  response = h2ac_traffic.post("/app/v1/invalid-utf8/text", b"ab\xffcd", {"content-type": "text/plain"})
  assert response["status"] == 200

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  assert response["status"] == 200
  uris = sorted(item["uri"] for item in response["body"])
  assert uris == ["/app/v1/invalid-utf8/other", "/app/v1/invalid-utf8/text"]

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/invalid-utf8/text&eventNumber=1&eventPath=/requestBody")
  assert response["status"] == 200
  assert response["body"] == "ab" + REPLACEMENT + "cd"


@pytest.mark.server
def test_003_sequence_and_summary_answer(h2ac_admin):

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/sequence")
  assert response["status"] == 200
  assert len(response["body"]) == 4  # recv + send per event

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/summary")
  assert response["status"] == 200
  assert response["body"]["totalEvents"] == 2
