import pytest
import json
from conftest import ADMIN_LOGGING_URI


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


def test_001_i_want_to_configure_invalid_logging_level(h2ac_admin):

  # Send PUT
  response = h2ac_admin.put(ADMIN_LOGGING_URI + '?level=invalidlevel')
  h2ac_admin.assert_response__status_body_headers(response, 400, "")


@pytest.mark.admin
def test_002_i_want_to_configure_informational_logging_level(h2ac_admin):

  # Send PUT
  response = h2ac_admin.put(ADMIN_LOGGING_URI + '?level=Informational')
  h2ac_admin.assert_response__status_body_headers(response, 200, "")

  # Send GET
  response = h2ac_admin.get(ADMIN_LOGGING_URI)
  assert response["status"] == 200
  assert response["body"] == "Informational"

  # Send PUT (restore Warning level)
  response = h2ac_admin.put(ADMIN_LOGGING_URI + '?level=Warning')
  h2ac_admin.assert_response__status_body_headers(response, 200, "")

  # Send GET
  response = h2ac_admin.get(ADMIN_LOGGING_URI)
  assert response["status"] == 200
  assert response["body"] == "Warning"
