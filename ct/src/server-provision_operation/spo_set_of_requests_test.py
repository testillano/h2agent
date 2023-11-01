import pytest
import json
from conftest import BASIC_FOO_BAR_SERVER_PROVISION_TEMPLATE, string2dict, ADMIN_SERVER_PROVISION_URI, VALID_SERVER_PROVISION__RESPONSE_BODY


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_i_want_to_provision_a_set_of_requests_on_admin_interface(admin_server_provision):

  for id in range(5):
    admin_server_provision(string2dict(BASIC_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=id), responseBodyRef=VALID_SERVER_PROVISION__RESPONSE_BODY)


@pytest.mark.admin
def test_002_i_want_to_retrieve_current_provisions_on_admin_interface(admin_server_matching, h2ac_admin):

  # Configure to have provisions ordered:
  admin_server_matching({ "algorithm":"RegexMatching" })

  # Send GET
  response = h2ac_admin.get(ADMIN_SERVER_PROVISION_URI)

  # Verify response
  assert response["status"] == 200
  for id in range(5):
    response_dict = response["body"][id]
    assert response_dict == string2dict(BASIC_FOO_BAR_SERVER_PROVISION_TEMPLATE, id=id)

