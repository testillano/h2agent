import pytest
import json
import subprocess
import threading
import time
import os
from conftest import ADMIN_VAULT_URI, RestClient, H2AGENT_ENDPOINT__admin

SSE_URI = "/admin/v1/vault/events"
ADMIN_URL = "http://" + H2AGENT_ENDPOINT__admin


def start_sse_listener(keys=None):
    """Start curl subprocess to read SSE stream. Returns (process, output_list)."""
    url = ADMIN_URL + SSE_URI
    if keys:
        url += "?" + "&".join(f"key={k}" for k in keys)

    proc = subprocess.Popen(
        ["curl", "--http2-prior-knowledge", "-N", "-s", url],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    return proc


def collect_events(proc, timeout=3):
    """Terminate curl and parse SSE events from its stdout."""
    time.sleep(timeout)
    proc.terminate()
    stdout, _ = proc.communicate(timeout=2)
    data = stdout.decode('utf-8')

    events = []
    for block in data.split("\n\n"):
        block = block.strip()
        if not block:
            continue
        for line in block.split("\n"):
            if line.startswith("data: "):
                events.append(json.loads(line[6:]))
    return events


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):
    admin_cleanup()


@pytest.mark.admin
def test_001_sse_receives_vault_event(h2ac_admin):
    """Subscribe to SSE with key filter, set vault key, verify event received"""
    key = "SSE_TEST_KEY"
    proc = start_sse_listener(keys=[key])
    time.sleep(0.5)

    h2ac_admin.postDict(ADMIN_VAULT_URI, {key: "hello"})

    events = collect_events(proc, timeout=1.5)
    assert len(events) >= 1
    assert events[0]["key"] == key
    assert events[0]["value"] == "hello"


@pytest.mark.admin
def test_002_sse_filters_by_key(h2ac_admin):
    """SSE connection watching key A should NOT receive events for key B"""
    watched_key = "SSE_WATCHED"
    other_key = "SSE_OTHER"

    proc = start_sse_listener(keys=[watched_key])
    time.sleep(0.5)

    h2ac_admin.postDict(ADMIN_VAULT_URI, {other_key: "noise"})
    time.sleep(0.3)
    h2ac_admin.postDict(ADMIN_VAULT_URI, {watched_key: "signal"})

    events = collect_events(proc, timeout=1.5)

    watched_events = [e for e in events if e["key"] == watched_key]
    other_events = [e for e in events if e["key"] == other_key]
    assert len(watched_events) >= 1
    assert watched_events[0]["value"] == "signal"
    assert len(other_events) == 0


@pytest.mark.admin
def test_003_sse_no_filter_receives_all(h2ac_admin):
    """SSE connection with no key filter receives all vault events"""
    proc = start_sse_listener()
    time.sleep(0.5)

    h2ac_admin.postDict(ADMIN_VAULT_URI, {"KEY_A": "val_a"})
    time.sleep(0.2)
    h2ac_admin.postDict(ADMIN_VAULT_URI, {"KEY_B": "val_b"})

    events = collect_events(proc, timeout=1.5)

    keys_received = {e["key"] for e in events}
    assert "KEY_A" in keys_received
    assert "KEY_B" in keys_received
