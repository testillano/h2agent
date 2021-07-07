import pytest
import json


@pytest.mark.admin
@pytest.mark.skip(reason="DELETE /admin/v1/server-provision is already implemented")
def test_001_i_want_to_force_not_implemented_error_towards_h2agent_admin_api(h2ac_admin):

  # Send POST
  response = h2ac_admin.request('DELETE', "/admin/v1/server-provision")

  # Verify response
  assert response["status"] == 204

