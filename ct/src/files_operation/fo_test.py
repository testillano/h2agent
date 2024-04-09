import pytest
import json
#import time
from conftest import ADMIN_FILES_URI, string2dict, FILE_MANAGER_PROVISION


@pytest.mark.admin
def test_001_i_want_to_get_process_files(h2ac_admin, admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(FILE_MANAGER_PROVISION))

  # Check file before traffic: skipped because the test could re-run
  #response = h2ac_admin.get(ADMIN_FILES_URI)
  #assert response["status"] == 204

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar")
  h2ac_admin.assert_response__status_body_headers(response, 200, "hello")

#  # Check file
#  response = h2ac_admin.get(ADMIN_FILES_URI)
#  responseBodyRef = [{ "bytes":0, "path": "/tmp/example.txt", "state": "opened" }]
#  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)
#
#  # Wait 2 seconds (long-term file closes in 1 second by default)
#  time.sleep(2)
#
  # Check files json
  response = h2ac_admin.get(ADMIN_FILES_URI)
  responseBodyRef = [{ "bytes":5, "path": "/tmp/example.txt", "state": "closed" }]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

