import pytest
import json
import os
import time
from conftest import VALID_SERVER_PROVISIONS__RESPONSE_BODY, ADMIN_SERVER_DATA_URI, ADMIN_CLIENT_ENDPOINT_URI, ADMIN_CLIENT_PROVISION_URI, ADMIN_CLIENT_DATA_URI, ADMIN_SERVER_PROVISION_URI, ADMIN_GLOBAL_VARIABLE_URI


H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))


# ============================================================
# Server-side: recvseq source + eraser target
# ============================================================

@pytest.mark.transform
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.transform
def test_001_provision_seq_addressing(admin_server_provision):

  admin_server_provision("seq_addressing_test.provision.json", responseBodyRef=VALID_SERVER_PROVISIONS__RESPONSE_BODY)


@pytest.mark.transform
def test_002_server_recvseq_source_and_eraser(h2ac_traffic, h2ac_admin):

  # Create an event (GET captures recvseq into globalVar.lastRecvSeq)
  response = h2ac_traffic.get("/app/v1/seq-test/data")
  h2ac_traffic.assert_response__status_body_headers(response, 200, {"origin": "seq-test"})

  # Verify event exists
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/seq-test/data")
  assert response["status"] == 200

  # Read event by recvseq (source addressing) — should return responseBody of the GET event
  response = h2ac_traffic.postDict("/app/v1/seq-test/read-by-seq", {})
  assert response["status"] == 200
  body = response["body"]
  assert isinstance(body, dict), f"Expected dict, got {type(body)}: {body}"
  assert "readResult" in body, f"Transform failed, body is: {body}"
  assert body["readResult"] == {"origin": "seq-test"}

  # Erase event by recvseq (eraser target)
  response = h2ac_traffic.postDict("/app/v1/seq-test/erase-by-seq", {})
  assert response["status"] == 200

  # Verify event was erased
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=/app/v1/seq-test/data")
  assert response["status"] == 204


# ============================================================
# Client-side: sendseq source + eraser target
# ============================================================

@pytest.mark.transform
def test_010_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.transform
def test_011_provision_client_seq_addressing(h2ac_admin):

  # Server responder
  server_provision = {
    "requestMethod": "POST",
    "requestUri": "/app/v1/seq-test/create",
    "responseCode": 201,
    "responseBody": {"id": 42},
    "responseHeaders": {"content-type": "application/json"}
  }
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, server_provision)
  assert response["status"] == 201

  # Loopback client endpoint
  endpoint = {"id": "seqtest", "host": H2AGENT_HOST, "port": H2AGENT_TRAFFIC_PORT, "secure": False, "permit": True}
  response = h2ac_admin.postDict(ADMIN_CLIENT_ENDPOINT_URI, endpoint)
  assert response["status"] == 201

  # Client provision: 2-step chain
  # Step 1: POST /create → capture sendseq, transition to step2
  # Step 2: read step1's event by sendseq, then erase it by sendseq
  step1 = {
    "id": "seqFlow",
    "endpoint": "seqtest",
    "requestMethod": "POST",
    "requestUri": "/app/v1/seq-test/create",
    "requestHeaders": {"content-type": "application/json"},
    "requestBody": {"name": "test"},
    "expectedResponseStatusCode": 201,
    "onResponseTransform": [
      {
        "source": "sendseq",
        "target": "var.step1Seq"
      }
    ],
    "outState": "step2"
  }
  step2 = {
    "id": "seqFlow",
    "inState": "step2",
    "endpoint": "seqtest",
    "requestMethod": "POST",
    "requestUri": "/app/v1/seq-test/create",
    "requestHeaders": {"content-type": "application/json"},
    "requestBody": {"name": "verify"},
    "expectedResponseStatusCode": 201,
    "transform": [
      {
        "source": "clientEvent.clientEndpointId=seqtest&requestMethod=POST&requestUri=/app/v1/seq-test/create&sendseq=@{step1Seq}&eventPath=/responseBody",
        "target": "globalVar.readBySeqResult"
      },
      {
        "source": "eraser",
        "target": "clientEvent.clientEndpointId=seqtest&requestMethod=POST&requestUri=/app/v1/seq-test/create&sendseq=@{step1Seq}"
      }
    ],
    "outState": "purge"
  }
  response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, [step1, step2])
  assert response["status"] == 201


@pytest.mark.transform
def test_012_client_sendseq_source_and_eraser(h2ac_admin):

  # Trigger the chain
  response = h2ac_admin.get(ADMIN_CLIENT_PROVISION_URI + "/seqFlow")
  assert response["status"] == 200

  # Wait for async chain completion
  time.sleep(2)

  # The chain purge (outState: "purge") clears step2's event.
  # Step1's event was already erased by the eraser target in step2's transform.
  # So ALL client data should be gone.
  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI)
  assert response["status"] == 204

  # Verify the globalVar was set (proves clientEvent source with sendseq worked)
  response = h2ac_admin.get(ADMIN_GLOBAL_VARIABLE_URI + "?name=readBySeqResult")
  assert response["status"] == 200
