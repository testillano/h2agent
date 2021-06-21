import pytest
import json


@pytest.mark.admin
def test_001_cleanup_provisions(resources, h2ac_admin):
  response = h2ac_admin.delete("/provision/v1/server-provisions")


@pytest.mark.admin
def test_002_i_want_to_provision_a_set_of_requests_on_admin_interface(resources, h2ac_admin):

  for id in range(5):
    requestBody = resources("server-provision_OK.json.in").format(id=id)

    # Send POST
    response = h2ac_admin.post("/provision/v1/server-provision", requestBody)

    # Verify response (status code is enough)
    assert response["status"] == 201

@pytest.mark.admin
def test_003_i_want_to_retrieve_current_provisions_on_admin_interface(resources, h2ac_admin):

  # Configure to have provisions ordered:
  requestBody = { "algorithm":"PriorityMatchingRegex" }
  responseBodyRef = { "result":"true", "response":"server-matching operation; valid schema and matching data received" }
  response = h2ac_admin.postDict("/provision/v1/server-matching", requestBody)
  h2ac_admin.assert_response__status_body_headers(response, 201, responseBodyRef)

  # Send GET
  response = h2ac_admin.get("/provision/v1/server-provisions")

  # Verify response
  assert response["status"] == 200
  for id in range(5):
    response_dict = response["body"][id]
    ref_dict = json.loads(resources("server-provision_OK.json.in").format(id=id))
    assert response_dict == ref_dict

