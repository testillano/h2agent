import pytest
import json
import threading
import time
from conftest import ADMIN_GLOBAL_VARIABLE_URI, RestClient, H2AGENT_ENDPOINT__admin


WAIT_URI_PREFIX = ADMIN_GLOBAL_VARIABLE_URI + "/"


def wait_uri(key, timeout_ms=None, value=None):
    q = []
    if timeout_ms is not None:
        q.append("timeoutMs={}".format(timeout_ms))
    if value is not None:
        q.append("value={}".format(value))
    uri = WAIT_URI_PREFIX + key + "/wait"
    if q:
        uri += "?" + "&".join(q)
    return uri


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):
    admin_cleanup()


@pytest.mark.admin
def test_001_wait_specific_value_already_satisfied(h2ac_admin):
    """Variable already has the target value => immediate 200"""
    h2ac_admin.postDict(ADMIN_GLOBAL_VARIABLE_URI, {"MY_KEY": "done"})

    response = h2ac_admin.get(wait_uri("MY_KEY", timeout_ms=1000, value="done"))
    assert response["status"] == 200
    body = response["body"]
    assert body["result"] == True
    assert body["key"] == "MY_KEY"
    assert body["value"] == "done"


@pytest.mark.admin
def test_002_wait_timeout(h2ac_admin):
    """Variable never changes => 408 after short timeout"""
    h2ac_admin.postDict(ADMIN_GLOBAL_VARIABLE_URI, {"STUCK": "initial"})

    response = h2ac_admin.get(wait_uri("STUCK", timeout_ms=200, value="never"))
    assert response["status"] == 408
    body = response["body"]
    assert body["result"] == False
    assert body["key"] == "STUCK"
    assert body["value"] == "initial"


@pytest.mark.admin
def test_003_wait_any_change(h2ac_admin):
    """Wait for any change: set variable from another connection"""
    h2ac_admin.postDict(ADMIN_GLOBAL_VARIABLE_URI, {"SIGNAL": "before"})

    result = {}

    def waiter():
        # hyper HTTP/2 connections are not thread-safe: a blocking GET on the
        # shared h2ac_admin would prevent other requests from being sent on
        # the same connection. Use a dedicated connection for the waiter.
        h2ac_wait = RestClient(H2AGENT_ENDPOINT__admin)
        result["response"] = h2ac_wait.get(wait_uri("SIGNAL", timeout_ms=5000))
        h2ac_wait.close()

    t = threading.Thread(target=waiter)
    t.start()

    time.sleep(0.3)
    h2ac_admin.postDict(ADMIN_GLOBAL_VARIABLE_URI, {"SIGNAL": "after"})

    t.join(timeout=6)
    assert not t.is_alive()

    response = result["response"]
    assert response["status"] == 200
    body = response["body"]
    assert body["result"] == True
    assert body["previousValue"] == "before"
    assert body["value"] == "after"


@pytest.mark.admin
def test_004_wait_specific_value_via_traffic(h2ac_admin, h2ac_traffic, admin_server_provision):
    """Wait for specific value set by traffic (server provision transform)"""
    provision = {
        "requestMethod": "POST",
        "requestUri": "/app/v1/signal",
        "responseCode": 200,
        "transform": [
            {
                "source": "value.ready",
                "target": "var.TRAFFIC_SIGNAL"
            }
        ]
    }
    admin_server_provision(provision)

    result = {}

    def waiter():
        h2ac_wait = RestClient(H2AGENT_ENDPOINT__admin)
        result["response"] = h2ac_wait.get(wait_uri("TRAFFIC_SIGNAL", timeout_ms=5000, value="ready"))
        h2ac_wait.close()

    t = threading.Thread(target=waiter)
    t.start()

    time.sleep(0.3)
    h2ac_traffic.post("/app/v1/signal")

    t.join(timeout=6)
    assert not t.is_alive()

    response = result["response"]
    assert response["status"] == 200
    assert response["body"]["result"] == True
    assert response["body"]["value"] == "ready"
