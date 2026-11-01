import pytest
import os
import time
from conftest import (
    ADMIN_CLIENT_PROVISION_URI, ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI,
    VALID_CLIENT_ENDPOINT__RESPONSE_BODY, string2dict, CLIENT_ENDPOINT_TEMPLATE
)

H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))
LOOPBACK_ENDPOINT = string2dict(CLIENT_ENDPOINT_TEMPLATE, id="loopback", port=H2AGENT_TRAFFIC_PORT)
LOOPBACK_ENDPOINT["host"] = H2AGENT_HOST

SERVER_PROVISION = {"requestMethod": "POST", "requestUri": "/app/v1/range", "responseCode": 200}

CLIENT_PROVISION = {
    "id": "rangeFlow",
    "endpoint": "loopback",
    "requestMethod": "POST",
    "requestUri": "/app/v1/range",
    "requestHeaders": {"content-type": "application/json"},
    "transform": [
        {"from": {"source": "var", "name": "sequence"}, "filter": [{"op": "cast", "type": "integer"}], "to": {"target": "requestBody", "path": "/seq"}}
    ]
}


def sent_sequences(h2ac_admin):
    response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=POST&requestUri=/app/v1/range")
    if response["status"] != 200: return []
    return [e["requestBody"]["seq"] for e in response["body"]["events"]]


@pytest.mark.client
def test_001_timer_trigger_sends_exactly_the_range(admin_cleanup, h2ac_admin, admin_client_endpoint):
    admin_cleanup()
    admin_client_endpoint(LOOPBACK_ENDPOINT, responseBodyRef=VALID_CLIENT_ENDPOINT__RESPONSE_BODY)
    assert h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, SERVER_PROVISION)["status"] == 201
    assert h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, CLIENT_PROVISION)["status"] == 201

    response = h2ac_admin.post(ADMIN_CLIENT_PROVISION_URI + "/rangeFlow/trigger?sequenceBegin=3&sequenceEnd=6&cps=20")
    assert response["status"] == 202

    deadline = time.time() + 5
    while time.time() < deadline and len(sent_sequences(h2ac_admin)) < 4: time.sleep(0.1)
    time.sleep(0.5)  # a fifth (wrong) request would arrive meanwhile

    assert sent_sequences(h2ac_admin) == [3, 4, 5, 6]

    response = h2ac_admin.get(ADMIN_CLIENT_PROVISION_URI)
    dynamics = [p for p in response["body"] if p["id"] == "rangeFlow"][0]["dynamics"]
    assert dynamics["sequence"] > dynamics["sequenceEnd"]  # done
