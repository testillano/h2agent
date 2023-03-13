import pytest
import json
from conftest import ADMIN_CONFIGURATION_URI


@pytest.mark.admin
def test_001_i_want_to_get_general_process_configuration(h2ac_admin):

  # Check configuration
  response = h2ac_admin.get(ADMIN_CONFIGURATION_URI)
  responseBodyRef = { "longTermFilesCloseDelayUsecs": 1000000, "shortTermFilesCloseDelayUsecs": 0 }
  h2ac_admin.assert_response__status_body_headers(response, 200, responseBodyRef)

