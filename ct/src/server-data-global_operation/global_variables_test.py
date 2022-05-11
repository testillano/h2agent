import pytest
import json
from conftest import GLOBAL_VARIABLES_1_2_3, GLOBAL_VARIABLES_PROVISION_TEMPLATE_GVARCREATED_GVARREMOVED_GVARANSWERED, string2dict, ADMIN_DATA_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_check_global_data_on_admin_interface(h2ac_admin, admin_server_data_global):

  # Check that there is no global variables server data here
  response = h2ac_admin.get(ADMIN_DATA_URI + '/global')
  assert response["status"] == 204

  # Configure and check global variables:
  admin_server_data_global(string2dict(GLOBAL_VARIABLES_1_2_3))
  response = h2ac_admin.get(ADMIN_DATA_URI + '/global')
  h2ac_admin.assert_response__status_body_headers(response, 200, string2dict(GLOBAL_VARIABLES_1_2_3))

  # Delete and check again:
  response = h2ac_admin.delete(ADMIN_DATA_URI)
  assert response["status"] == 200
  response = h2ac_admin.delete(ADMIN_DATA_URI)
  assert response["status"] == 204

  response = h2ac_admin.get(ADMIN_DATA_URI + '/global')
  h2ac_admin.assert_response__status_body_headers(response, 204, "")

@pytest.mark.admin
def test_002_i_want_to_check_global_data_on_admin_interface_due_to_traffic(h2ac_admin, h2ac_traffic, admin_provision, admin_server_data_global):

  # Configure global variables var1, var2 and var3:
  admin_server_data_global(string2dict(GLOBAL_VARIABLES_1_2_3))

  # Provision
  admin_provision(string2dict(GLOBAL_VARIABLES_PROVISION_TEMPLATE_GVARCREATED_GVARREMOVED_GVARANSWERED, gvarcreated = "var4", gvarremoved = "var2", gvaranswered = "varMissing"))

  # Traffic and global variables
  response = h2ac_traffic.post("/app/v1/foo/bar")
  h2ac_traffic.assert_response__status_body_headers(response, 200, { "foo":"bar" })
  response = h2ac_admin.get(ADMIN_DATA_URI + '/global')
  h2ac_admin.assert_response__status_body_headers(response, 200, { "var1":"value1", "var3":"value3", "var4":"var4value" })

  # Provision
  admin_provision(string2dict(GLOBAL_VARIABLES_PROVISION_TEMPLATE_GVARCREATED_GVARREMOVED_GVARANSWERED, gvarcreated = "var2", gvarremoved = "var1", gvaranswered = "var4"))

  # Traffic and global variables
  response = h2ac_traffic.post("/app/v1/foo/bar")
  h2ac_traffic.assert_response__status_body_headers(response, 200, { "foo":"bar", "gvaranswered":"var4value" })
  response = h2ac_admin.get(ADMIN_DATA_URI + '/global')
  h2ac_admin.assert_response__status_body_headers(response, 200, { "var2":"var2value", "var3":"value3", "var4":"var4value" })

