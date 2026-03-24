import pytest
import json
from conftest import VAULT_1_2_3, VAULT_PROVISION_TEMPLATE_GVARCREATED_GVARREMOVED_GVARANSWERED, string2dict, ADMIN_VAULT_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_check_global_data_on_admin_interface(h2ac_admin, admin_vault):

  # Check that there is no vault here
  response = h2ac_admin.get(ADMIN_VAULT_URI)
  assert response["status"] == 204

  # Configure and check vault:
  admin_vault(string2dict(VAULT_1_2_3))
  response = h2ac_admin.get(ADMIN_VAULT_URI)
  h2ac_admin.assert_response__status_body_headers(response, 200, string2dict(VAULT_1_2_3))

  # Now, get specific variable:
  response = h2ac_admin.get(ADMIN_VAULT_URI + "?name=var1")
  h2ac_admin.assert_response__status_body_headers(response, 200, "value1")

  # Delete var1 twice to check 200 and 204:
  response = h2ac_admin.delete(ADMIN_VAULT_URI + "?name=var1")
  assert response["status"] == 200
  response = h2ac_admin.delete(ADMIN_VAULT_URI + "?name=var1")
  assert response["status"] == 204

  # Global deletion twice to check 200 and 204:
  response = h2ac_admin.delete(ADMIN_VAULT_URI)
  assert response["status"] == 200
  response = h2ac_admin.delete(ADMIN_VAULT_URI)
  assert response["status"] == 204

  response = h2ac_admin.get(ADMIN_VAULT_URI)
  h2ac_admin.assert_response__status_body_headers(response, 204, "")

@pytest.mark.admin
def test_002_i_want_to_check_global_data_on_admin_interface_due_to_traffic(h2ac_admin, h2ac_traffic, admin_server_provision, admin_vault):

  # Configure vault var1, var2 and var3:
  admin_vault(string2dict(VAULT_1_2_3))

  # Provision
  admin_server_provision(string2dict(VAULT_PROVISION_TEMPLATE_GVARCREATED_GVARREMOVED_GVARANSWERED, gvarcreated = "var4", gvarremoved = "var2", gvaranswered = "varMissing"))

  # Traffic and vault
  response = h2ac_traffic.post("/app/v1/foo/bar")
  h2ac_traffic.assert_response__status_body_headers(response, 200, { "foo":"bar" })
  response = h2ac_admin.get(ADMIN_VAULT_URI)
  h2ac_admin.assert_response__status_body_headers(response, 200, { "var1":"value1", "var3":"value3", "var4":"var4value" })

  # Provision
  admin_server_provision(string2dict(VAULT_PROVISION_TEMPLATE_GVARCREATED_GVARREMOVED_GVARANSWERED, gvarcreated = "var2", gvarremoved = "var1", gvaranswered = "var4"))

  # Traffic and vault
  response = h2ac_traffic.post("/app/v1/foo/bar")
  h2ac_traffic.assert_response__status_body_headers(response, 200, { "foo":"bar", "gvaranswered":"var4value" })
  response = h2ac_admin.get(ADMIN_VAULT_URI)
  h2ac_admin.assert_response__status_body_headers(response, 200, { "var2":"var2value", "var3":"value3", "var4":"var4value" })

