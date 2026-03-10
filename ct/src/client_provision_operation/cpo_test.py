import pytest
import json
import os
import time
from conftest import (
    ADMIN_CLIENT_ENDPOINT_URI, ADMIN_CLIENT_PROVISION_URI, ADMIN_CLIENT_DATA_URI,
    ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI,
    VALID_CLIENT_ENDPOINT__RESPONSE_BODY, VALID_CLIENT_PROVISION__RESPONSE_BODY,
    VALID_CLIENT_PROVISIONS__RESPONSE_BODY,
    string2dict, CLIENT_ENDPOINT_TEMPLATE
)

# Loopback endpoint: h2agent traffic port, reachable via service ClusterIP
H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))
LOOPBACK_ENDPOINT = string2dict(CLIENT_ENDPOINT_TEMPLATE, id="loopback", port=H2AGENT_TRAFFIC_PORT)
LOOPBACK_ENDPOINT["host"] = H2AGENT_HOST

# Server provision: responds to GET /app/v1/hello with {"greeting":"hello world"}
SERVER_PROVISION_HELLO = {
    "requestMethod": "GET",
    "requestUri": "/app/v1/hello",
    "responseCode": 200,
    "responseBody": {"greeting": "hello world"},
    "responseHeaders": {"content-type": "application/json"}
}

# Server provision: responds to POST /app/v1/goodbye with {"farewell":"see you"}
SERVER_PROVISION_GOODBYE = {
    "requestMethod": "POST",
    "requestUri": "/app/v1/goodbye",
    "responseCode": 200,
    "responseBody": {"farewell": "see you"},
    "responseHeaders": {"content-type": "application/json"}
}


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):
    admin_cleanup()


# ==================== BASIC FLOW ====================

@pytest.mark.admin
def test_001_configure_loopback_endpoint(h2ac_admin, admin_client_endpoint):
    admin_client_endpoint(LOOPBACK_ENDPOINT, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)

    response = h2ac_admin.get(ADMIN_CLIENT_ENDPOINT_URI)
    h2ac_admin.assert_response__status_body_headers(response, 200,
        [{"host": H2AGENT_HOST, "id": "loopback", "permit": True, "port": H2AGENT_TRAFFIC_PORT, "secure": False}])


@pytest.mark.admin
def test_002_configure_server_provisions(h2ac_admin):
    response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, [SERVER_PROVISION_HELLO, SERVER_PROVISION_GOODBYE])
    assert response["status"] == 201


@pytest.mark.admin
def test_003_configure_basic_client_provision(h2ac_admin, admin_client_provision):
    provision = {
        "id": "basicFlow",
        "endpoint": "loopback",
        "expectedResponseStatusCode": 200,
        "requestMethod": "GET",
        "requestUri": "/app/v1/hello",
        "requestHeaders": {"content-type": "application/json"}
    }
    admin_client_provision(provision)


@pytest.mark.client
def test_004_trigger_basic_client_provision_and_verify(h2ac_admin):
    # Trigger
    response = h2ac_admin.get(ADMIN_CLIENT_PROVISION_URI + "/basicFlow")
    assert response["status"] == 200

    time.sleep(0.5)

    # Verify client event
    response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI)
    assert response["status"] == 200
    events = response["body"]
    assert len(events) == 1
    assert events[0]["method"] == "GET"
    assert events[0]["uri"] == "/app/v1/hello"
    event = events[0]["events"][0]
    assert event["responseBody"] == {"greeting": "hello world"}
    assert event["clientProvisionId"] == "basicFlow"

    # Verify server received the request
    response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
    assert response["status"] == 200
    server_events = response["body"]
    hello_events = [e for e in server_events if e["uri"] == "/app/v1/hello"]
    assert len(hello_events) == 1


# ==================== STATE PROGRESSION (CHAINED FLOW) ====================

@pytest.mark.admin
def test_005_cleanup_for_chained_flow(admin_cleanup):
    admin_cleanup()


@pytest.mark.admin
def test_006_configure_chained_flow(h2ac_admin, admin_client_endpoint, admin_client_provision):
    # Endpoint
    admin_client_endpoint(LOOPBACK_ENDPOINT, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)

    # Server provisions
    response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, [SERVER_PROVISION_HELLO, SERVER_PROVISION_GOODBYE])
    assert response["status"] == 201

    # Client provision step1: GET /hello, outState=step2
    step1 = {
        "id": "chainedFlow",
        "endpoint": "loopback",
        "expectedResponseStatusCode": 200,
        "requestMethod": "GET",
        "requestUri": "/app/v1/hello",
        "requestHeaders": {"content-type": "application/json"},
        "outState": "step2"
    }
    # Client provision step2: POST /goodbye, inState=step2
    step2 = {
        "id": "chainedFlow",
        "inState": "step2",
        "endpoint": "loopback",
        "expectedResponseStatusCode": 200,
        "requestMethod": "POST",
        "requestUri": "/app/v1/goodbye",
        "requestHeaders": {"content-type": "application/json"},
        "requestBody": {"message": "done"}
    }
    response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, [step1, step2])
    assert response["status"] == 201


@pytest.mark.client
def test_007_trigger_chained_flow_and_verify(h2ac_admin):
    # Trigger
    response = h2ac_admin.get(ADMIN_CLIENT_PROVISION_URI + "/chainedFlow")
    assert response["status"] == 200

    time.sleep(0.5)

    # Verify 2 client events
    response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI)
    assert response["status"] == 200
    events = response["body"]
    uris = {e["uri"] for e in events}
    assert "/app/v1/hello" in uris
    assert "/app/v1/goodbye" in uris

    # Verify server received both requests
    response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
    server_uris = {e["uri"] for e in response["body"]}
    assert "/app/v1/hello" in server_uris
    assert "/app/v1/goodbye" in server_uris


# ==================== TRANSFORM: globalVar PROPAGATION ====================

@pytest.mark.admin
def test_008_cleanup_for_transform_flow(admin_cleanup):
    admin_cleanup()


@pytest.mark.admin
def test_009_configure_transform_flow(h2ac_admin, admin_client_endpoint, admin_client_provision):
    admin_client_endpoint(LOOPBACK_ENDPOINT, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)

    response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, [SERVER_PROVISION_HELLO, SERVER_PROVISION_GOODBYE])
    assert response["status"] == 201

    step1 = {
        "id": "transformFlow",
        "endpoint": "loopback",
        "expectedResponseStatusCode": 200,
        "requestMethod": "GET",
        "requestUri": "/app/v1/hello",
        "requestHeaders": {"content-type": "application/json"},
        "outState": "step2",
        "onResponseTransform": [
            {"source": "response.body", "target": "globalVar.helloResponse"}
        ]
    }
    step2 = {
        "id": "transformFlow",
        "inState": "step2",
        "endpoint": "loopback",
        "expectedResponseStatusCode": 200,
        "requestMethod": "POST",
        "requestUri": "/app/v1/goodbye",
        "requestHeaders": {"content-type": "application/json"},
        "transform": [
            {"source": "globalVar.helloResponse", "target": "request.body.json.jsonstring"}
        ]
    }
    response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, [step1, step2])
    assert response["status"] == 201


@pytest.mark.client
def test_010_trigger_transform_flow_and_verify_body_propagation(h2ac_admin):
    response = h2ac_admin.get(ADMIN_CLIENT_PROVISION_URI + "/transformFlow")
    assert response["status"] == 200

    time.sleep(0.5)

    # Verify POST /goodbye carried the body from GET /hello response
    response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI)
    events = response["body"]
    goodbye_events = [e for e in events if e["uri"] == "/app/v1/goodbye"]
    assert len(goodbye_events) == 1
    event = goodbye_events[0]["events"][0]
    assert event["requestBody"] == {"greeting": "hello world"}


# ==================== SERVER-TRIGGERED CLIENT FLOW ====================

@pytest.mark.admin
def test_011_cleanup_for_server_triggered_flow(admin_cleanup):
    admin_cleanup()


@pytest.mark.admin
def test_012_configure_server_triggered_flow(h2ac_admin, admin_client_endpoint, admin_client_provision):
    admin_client_endpoint(LOOPBACK_ENDPOINT, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)

    # Server provision: receives webhook, triggers client flow
    server_webhook = {
        "requestMethod": "POST",
        "requestUri": "/app/v1/webhook",
        "responseCode": 200,
        "responseBody": {"status": "received"},
        "responseHeaders": {"content-type": "application/json"},
        "transform": [
            {"source": "request.body", "target": "globalVar.webhookBody"},
            {"source": "value.1", "target": "clientProvision.forwardFlow.initial"}
        ]
    }
    # Server provision: receives forwarded request
    server_forward = {
        "requestMethod": "POST",
        "requestUri": "/app/v1/forward",
        "responseCode": 200,
        "responseBody": {"status": "forwarded"},
        "responseHeaders": {"content-type": "application/json"}
    }
    response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, [server_webhook, server_forward])
    assert response["status"] == 201

    # Client provision: forwards the webhook body
    forward_provision = {
        "id": "forwardFlow",
        "endpoint": "loopback",
        "expectedResponseStatusCode": 200,
        "requestMethod": "POST",
        "requestUri": "/app/v1/forward",
        "requestHeaders": {"content-type": "application/json"},
        "transform": [
            {"source": "globalVar.webhookBody", "target": "request.body.json.jsonstring"}
        ]
    }
    admin_client_provision(forward_provision)


@pytest.mark.client
def test_013_send_webhook_and_verify_client_triggered(h2ac_traffic, h2ac_admin):
    # External POST to webhook
    response = h2ac_traffic.postDict("/app/v1/webhook", {"event": "user.created", "userId": 99})
    h2ac_traffic.assert_response__status_body_headers(response, 200, {"status": "received"})

    time.sleep(0.5)

    # Verify client event was triggered
    response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI)
    assert response["status"] == 200
    events = response["body"]
    forward_events = [e for e in events if e["uri"] == "/app/v1/forward"]
    assert len(forward_events) == 1
    event = forward_events[0]["events"][0]
    assert event["requestBody"] == {"event": "user.created", "userId": 99}
