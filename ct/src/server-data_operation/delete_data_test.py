import pytest
import json
from conftest import ADMIN_SERVER_DATA_URI, assertUnprovisioned


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_delete_partial_internal_data_after_storing_some_traffic_events(h2ac_admin, h2ac_traffic):

  # Send traffic without any provision (no need for more, as we will delete anyway and indexes does not depend
  # on response information):
  response = h2ac_traffic.get("/app/v1/foo/bar/1")

  # Get this first server sequence:
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  seq_ini = response["body"][0]["requests"][0]["serverSequence"]

  response = h2ac_traffic.get("/app/v1/foo/bar/1") # this will be seq_ini + 1
  response = h2ac_traffic.postDict("/app/v1/foo/bar/2", { "foo-bar":"first" })  # this will be seq_ini + 2
  response = h2ac_traffic.postDict("/app/v1/foo/bar/2", { "foo-bar":"second" })  # this will be seq_ini + 3
  response = h2ac_traffic.postDict("/app/v1/foo/bar/2", { "foo-bar":"third" }) # this will be seq_ini + 4

  # We have 2 GETs and 3 POSTs

  # We will delete partially both, for example, the last one of GETs and POSTs:
  response = h2ac_admin.delete(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar/1&requestNumber=-1")
  response["status"] = 200
  response = h2ac_admin.delete(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/foo/bar/2&requestNumber=-1")
  response["status"] = 200

  # Now we have 1 GET and 2 POSTs (those with 'first' and 'second' body values for 'foo-bar'):
  # Method + Uri key is not sort (only the requests within), so we have to search for them:

  # Locate the survival GET key:
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  survival_get_key = next(item for item in response["body"] if item["method"] == "GET")
  event = survival_get_key["requests"][0]
  assert survival_get_key["uri"] == "/app/v1/foo/bar/1"
  assertUnprovisioned(event, None, seq_ini) # we removed the last, so the survival is the first sequence

  # Locate the POSTs key:
  posts_key = next(item for item in response["body"] if item["method"] == "POST")
  event1 = posts_key["requests"][0]
  event2 = posts_key["requests"][1]
  assert posts_key["uri"] == "/app/v1/foo/bar/2"
  assertUnprovisioned(event1, { "foo-bar":"first" }, seq_ini + 2)
  assertUnprovisioned(event2, { "foo-bar":"second" }, seq_ini + 3)

  # Remove POSTs at a time:
  response = h2ac_admin.delete(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/foo/bar/2")
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  with pytest.raises(StopIteration): posts_key = next(item for item in response["body"] if item["method"] == "POST")

  # Remove the first (and only) GET:
  response = h2ac_admin.delete(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/foo/bar/1&requestNumber=1")
  response["status"] = 200

  # Even not GET survives, although the key remains (without requests node):
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  key = next(item for item in response["body"] if item["method"] == "GET")
  assert key["uri"] == "/app/v1/foo/bar/1"

