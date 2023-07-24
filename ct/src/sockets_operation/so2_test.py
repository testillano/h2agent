import pytest
import json
from conftest import ADMIN_SOCKETS_URI, string2dict, SOCKET_MANAGER_PROVISION


@pytest.mark.admin
def test_001_i_want_to_get_process_sockets(h2ac_admin, admin_server_provision, h2ac_traffic):

  # Provision
  admin_server_provision(string2dict(SOCKET_MANAGER_PROVISION))

  # Check file before traffic: skipped because the test could re-run
  #response = h2ac_admin.get(ADMIN_SOCKETS_URI)
  #assert response["status"] == 204

  # Send GET
  response = h2ac_traffic.get("/app/v1/foo/bar")
  h2ac_admin.assert_response__status_body_headers(response, 200, "")

  # Check sockets json
  response = h2ac_admin.get(ADMIN_SOCKETS_URI)
  responseBodyRef = [{ "socket":0, "path": "/tmp/my_unix_socket2" }, { "socket":0, "path": "/tmp/my_unix_socket1" }]
  responseBodyRef[0]["socket"] = response["body"][0]["socket"]
  responseBodyRef[1]["socket"] = response["body"][1]["socket"]
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

