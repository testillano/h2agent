import pytest
import json
from conftest import BASIC_FOO_BAR_PROVISION_TEMPLATE, string2dict, ADMIN_PROVISION_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_provision_a_set_of_requests_on_admin_interface(admin_provision):

  for id in range(5):
    admin_provision(string2dict(BASIC_FOO_BAR_PROVISION_TEMPLATE, id=id))

@pytest.mark.admin
def test_002_i_want_to_retrieve_current_provisions_on_admin_interface(admin_matching, h2ac_admin):

  # Configure to have provisions ordered:
  admin_matching({ "algorithm":"PriorityMatchingRegex" })

  # Send GET
  response = h2ac_admin.get(ADMIN_PROVISION_URI)

  # Verify response
  assert response["status"] == 200
  for id in range(5):
    response_dict = response["body"][id]
    assert response_dict == string2dict(BASIC_FOO_BAR_PROVISION_TEMPLATE, id=id)

