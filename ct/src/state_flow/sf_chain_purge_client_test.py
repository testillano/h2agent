import pytest
import json
import os
import time
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_CLIENT_ENDPOINT_URI, ADMIN_CLIENT_PROVISION_URI, ADMIN_CLIENT_DATA_URI


H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision_client_chain_purge(h2ac_admin):

  # Server provisions (responders)
  server_provisions = [
    {
      "requestMethod": "POST",
      "requestUri": "/app/v1/chain-purge/create",
      "responseCode": 201,
      "responseBody": {"id": 1},
      "responseHeaders": {"content-type": "application/json"}
    },
    {
      "requestMethod": "GET",
      "requestUri": "/app/v1/chain-purge/status/1",
      "responseCode": 200,
      "responseBody": {"ready": True},
      "responseHeaders": {"content-type": "application/json"}
    }
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, server_provisions)
  assert response["status"] == 201

  # Loopback client endpoint
  endpoint = {"id": "loopback", "host": H2AGENT_HOST, "port": H2AGENT_TRAFFIC_PORT, "secure": False, "permit": True}
  response = h2ac_admin.postDict(ADMIN_CLIENT_ENDPOINT_URI, endpoint)
  assert response["status"] == 201

  # Client provisions: 2-step chain ending in purge
  step1 = {
    "id": "chainPurgeFlow",
    "endpoint": "loopback",
    "requestMethod": "POST",
    "requestUri": "/app/v1/chain-purge/create",
    "requestHeaders": {"content-type": "application/json"},
    "requestBody": {"name": "test"},
    "expectedResponseStatusCode": 201,
    "outState": "step2"
  }
  step2 = {
    "id": "chainPurgeFlow",
    "inState": "step2",
    "endpoint": "loopback",
    "requestMethod": "GET",
    "requestUri": "/app/v1/chain-purge/status/1",
    "expectedResponseStatusCode": 200,
    "outState": "purge"
  }
  response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, [step1, step2])
  assert response["status"] == 201


@pytest.mark.client
def test_002_client_chain_purge_clears_all_steps(h2ac_admin):

  # Trigger the chain
  response = h2ac_admin.get(ADMIN_CLIENT_PROVISION_URI + "/chainPurgeFlow")
  assert response["status"] == 200

  # Wait for async chain completion
  time.sleep(2)

  # Verify ALL client data is purged (both POST and GET events)
  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI)
  assert response["status"] == 204
