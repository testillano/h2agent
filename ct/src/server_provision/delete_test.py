import pytest
import json


@pytest.mark.admin
def test_001_i_want_to_send_delete_server_provision_operation(resources, h2ac_admin):

  # Send POST
  response = h2ac_admin.delete("/provision/v1/server_provision")

  # Verify response
  assert response["status"] == 204

@pytest.mark.admin
def test_002_i_want_to_send_delete_for_unsupported_operation(resources, h2ac_admin):

  # Send POST
  response = h2ac_admin.delete("/provision/v1/foo")

  # Verify response
  assert response["status"] == 400
