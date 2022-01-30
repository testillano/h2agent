import pytest
import json
from conftest import ADMIN_DATA_URI, assertUnprovisionedServerDataItemRequestsIndex



@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_check_internal_data_after_unprovisioned_event(h2ac_admin, h2ac_traffic):

  # Send traffic without any provision
  response = h2ac_traffic.get("/app/v1/foo/bar/1")
  responseBodyRef = ""
  h2ac_traffic.assert_response__status_body_headers(response, 501, responseBodyRef)

  # Server data should be something like:
  #[
  #  {
  #    "method": "GET",
  #    "requests": [
  #      {
  #        "headers": {
  #          "accept": "*/*",
  #          "user-agent": "curl/7.58.0"
  #        },
  #        "previousState": "",
  #        "receptionTimestampMs": 1625996928191,
  #        "responseDelayMs": 0,
  #        "responseStatusCode": 501,
  #        "serverSequence": 1,
  #        "state": ""
  #      }
  #    ],
  #    "uri": "/app/v1/foo/bar/1"
  #  }
  #]
  # Check server data
  response = h2ac_admin.get(ADMIN_DATA_URI)
  assertUnprovisionedServerDataItemRequestsIndex(response["body"][0], "GET", "/app/v1/foo/bar/1", 0) # no body in our test

