import pytest
import json
import os
import time
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI, ADMIN_CLIENT_ENDPOINT_URI, ADMIN_CLIENT_PROVISION_URI, ADMIN_CLIENT_DATA_URI


# =============================================================================
# Coverage for the "per-state client-endpoint on chain progression" fix.
#
# Bug (pre-fix): a client-provision chain (single id, states linked by
# inState/outState) honored ONLY the endpoint of the first (triggered) link.
# Every subsequent link REUSED that resolved endpoint, silently ignoring its
# own 'endpoint' field. So a heterogeneous chain misrouted non-initial links.
#
# Why client-data alone cannot prove the fix: the client-data DataKey is built
# from the LINK's own clientEndpointId (captured from the provision, not from
# the routing endpoint). Therefore the second link's client-data shows its
# declared endpoint id BOTH pre- and post-fix. The only unambiguous
# discriminator is the PHYSICAL destination that received the second request.
#
# This module provides two complementary tests:
#
#   (A) test_dead_endpoint_* : runs on the CURRENT single-instance ct topology.
#       The second link targets a DEAD endpoint (closed port). Routing is the
#       discriminator via a transport error:
#         - pre-fix : link 'second' is misrouted to the (live) loopback -> 200,
#                     so a client-data event for the second URI EXISTS.
#         - post-fix: link 'second' honors the dead endpoint -> connection
#                     error (statusCode <= 0) -> chain breaks BEFORE loadEvent,
#                     so NO client-data event for the second URI exists.
#       (In sendClientRequest the transport-error branch returns before storing
#       the event, so absence of the second event is the post-fix signature.)
#
#   (B) test_two_instances_* : the canonical test from the proposal. Requires a
#       SECOND live h2agent instance exposed via H2AGENT_B_SERVICE_HOST /
#       H2AGENT_B_SERVICE_PORT_HTTP2_TRAFFIC / _ADMIN env vars (second subchart
#       in helm/ct-h2agent). Asserts the second link physically lands on
#       instance B server-data and NOT on instance A. Skipped until that infra
#       exists.
# =============================================================================

H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
H2AGENT_TRAFFIC_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC', 8000))

# A port nothing listens on inside the pod network: yields connection-refused
# (fast, deterministic) rather than a timeout. Must differ from the live traffic
# port so the second link cannot accidentally reach the live server.
DEAD_PORT = 8009
assert DEAD_PORT != H2AGENT_TRAFFIC_PORT, "DEAD_PORT must not collide with the live traffic port"

STEP_A_URI = "/app/v1/per-state/step-a"
STEP_B_URI = "/app/v1/per-state/step-b"


# ============================ (A) DEAD-ENDPOINT ==============================
# Discriminates the routing fix on the single-instance topology.

@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision_chain_with_dead_second_endpoint(h2ac_admin):

  # Server responders for BOTH URIs, on the single live instance.
  server_provisions = [
    {
      "requestMethod": "GET",
      "requestUri": STEP_A_URI,
      "responseCode": 200,
      "responseBody": {"step": "a"},
      "responseHeaders": {"content-type": "application/json"}
    },
    {
      "requestMethod": "POST",
      "requestUri": STEP_B_URI,
      "responseCode": 200,
      "responseBody": {"step": "b"},
      "responseHeaders": {"content-type": "application/json"}
    }
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, server_provisions)
  assert response["status"] == 201

  # Two client endpoints:
  #   epLive -> loopback traffic port (alive)
  #   epDead -> closed port (connection refused)
  endpoints = [
    {"id": "epLive", "host": H2AGENT_HOST, "port": H2AGENT_TRAFFIC_PORT, "secure": False, "permit": True},
    {"id": "epDead", "host": H2AGENT_HOST, "port": DEAD_PORT, "secure": False, "permit": True}
  ]
  response = h2ac_admin.postDict(ADMIN_CLIENT_ENDPOINT_URI, endpoints)
  assert response["status"] == 201

  # Chain: initial link -> epLive (step-a), second link -> epDead (step-b).
  step1 = {
    "id": "perStateDead",
    "endpoint": "epLive",
    "requestMethod": "GET",
    "requestUri": STEP_A_URI,
    "requestHeaders": {"content-type": "application/json"},
    "expectedResponseStatusCode": 200,
    "outState": "second"
  }
  step2 = {
    "id": "perStateDead",
    "inState": "second",
    "endpoint": "epDead",
    "requestMethod": "POST",
    "requestUri": STEP_B_URI,
    "requestHeaders": {"content-type": "application/json"},
    "requestBody": {"message": "second"}
  }
  response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, [step1, step2])
  assert response["status"] == 201


@pytest.mark.client
def test_002_second_link_honors_its_own_dead_endpoint(h2ac_admin):

  # Trigger the chain.
  response = h2ac_admin.get(ADMIN_CLIENT_PROVISION_URI + "/perStateDead")
  assert response["status"] == 200

  # Poll until the first link has produced its client event (chain started).
  # Chain progression is inline on the I/O thread, so once step-a is stored the
  # second link's send attempt (to the dead endpoint) has already been issued.
  events = []
  deadline = time.time() + 5.0
  while time.time() < deadline:
    response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI)
    if response["status"] == 200:
      events = response["body"]
      if any(e["uri"] == STEP_A_URI for e in events):
        break
    time.sleep(0.1)

  # Small settle margin so a (buggy) misrouted step-b would have time to land.
  time.sleep(1)
  response = h2ac_admin.get(ADMIN_CLIENT_DATA_URI)
  assert response["status"] == 200
  events = response["body"]

  # First link must always have executed and reached the live endpoint.
  step_a_events = [e for e in events if e["uri"] == STEP_A_URI]
  assert len(step_a_events) == 1, "first link (step-a) must have produced a client event"

  # Post-fix discriminator: the second link honors epDead -> connection error ->
  # chain breaks before storing the event, so NO client-data event for step-b.
  # Pre-fix, step-b was misrouted to epLive and would produce a 200 event here.
  step_b_events = [e for e in events if e["uri"] == STEP_B_URI]
  assert len(step_b_events) == 0, (
    "second link must be routed to its own (dead) endpoint; a step-b client "
    "event means it was misrouted to the inherited live endpoint (pre-fix bug)"
  )

  # Reinforce with the physical server-data: the dead-routed second request must
  # never have reached the live server.
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  assert response["status"] == 200
  server_uris = {e["uri"] for e in response["body"]}
  assert STEP_A_URI in server_uris, "live server must have received step-a"
  assert STEP_B_URI not in server_uris, (
    "live server received step-b: second link was misrouted to the inherited "
    "endpoint instead of its own (pre-fix bug)"
  )


@pytest.mark.admin
def test_003_cleanup(admin_cleanup):

  admin_cleanup()


# =========================== (B) TWO INSTANCES ==============================
# Canonical proposal test: requires a second live h2agent instance.

_B_HOST = os.environ.get('H2AGENT_B_SERVICE_HOST')
_B_TRAFFIC = os.environ.get('H2AGENT_B_SERVICE_PORT_HTTP2_TRAFFIC')
_B_ADMIN = os.environ.get('H2AGENT_B_SERVICE_PORT_HTTP2_ADMIN')

_two_instances_ready = all([_B_HOST, _B_TRAFFIC, _B_ADMIN])
_skip_reason = (
  "second h2agent instance not deployed: set H2AGENT_B_SERVICE_HOST, "
  "H2AGENT_B_SERVICE_PORT_HTTP2_TRAFFIC and H2AGENT_B_SERVICE_PORT_HTTP2_ADMIN "
  "(add a second h2agent subchart to helm/ct-h2agent)"
)


@pytest.fixture(scope='module')
def h2ac_admin_b():
  # Local import to avoid touching conftest for an optional fixture.
  from conftest import RestClient
  h2ac = RestClient(_B_HOST + ':' + str(_B_ADMIN))
  yield h2ac
  h2ac.close()


@pytest.mark.skipif(not _two_instances_ready, reason=_skip_reason)
@pytest.mark.admin
def test_010_cleanup_two_instances(admin_cleanup, h2ac_admin_b):

  admin_cleanup()
  # Cleanup instance B server-data/provisions too.
  h2ac_admin_b.delete(ADMIN_SERVER_PROVISION_URI)
  h2ac_admin_b.delete(ADMIN_SERVER_DATA_URI)


@pytest.mark.skipif(not _two_instances_ready, reason=_skip_reason)
@pytest.mark.admin
def test_011_provision_chain_across_two_instances(h2ac_admin, h2ac_admin_b):

  # Instance A responds step-a; instance B responds step-b.
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, {
    "requestMethod": "GET", "requestUri": STEP_A_URI, "responseCode": 200,
    "responseBody": {"step": "a"}, "responseHeaders": {"content-type": "application/json"}
  })
  assert response["status"] == 201
  response = h2ac_admin_b.postDict(ADMIN_SERVER_PROVISION_URI, {
    "requestMethod": "POST", "requestUri": STEP_B_URI, "responseCode": 200,
    "responseBody": {"step": "b"}, "responseHeaders": {"content-type": "application/json"}
  })
  assert response["status"] == 201

  # epA -> instance A traffic, epB -> instance B traffic.
  endpoints = [
    {"id": "epA", "host": H2AGENT_HOST, "port": H2AGENT_TRAFFIC_PORT, "secure": False, "permit": True},
    {"id": "epB", "host": _B_HOST, "port": int(_B_TRAFFIC), "secure": False, "permit": True}
  ]
  response = h2ac_admin.postDict(ADMIN_CLIENT_ENDPOINT_URI, endpoints)
  assert response["status"] == 201

  # Chain: initial -> epA (step-a), second -> epB (step-b).
  step1 = {
    "id": "multiEndpoint", "endpoint": "epA",
    "requestMethod": "GET", "requestUri": STEP_A_URI,
    "requestHeaders": {"content-type": "application/json"},
    "expectedResponseStatusCode": 200, "outState": "second"
  }
  step2 = {
    "id": "multiEndpoint", "inState": "second", "endpoint": "epB",
    "requestMethod": "POST", "requestUri": STEP_B_URI,
    "requestHeaders": {"content-type": "application/json"},
    "requestBody": {"message": "second"},
    "expectedResponseStatusCode": 200
  }
  response = h2ac_admin.postDict(ADMIN_CLIENT_PROVISION_URI, [step1, step2])
  assert response["status"] == 201


@pytest.mark.skipif(not _two_instances_ready, reason=_skip_reason)
@pytest.mark.client
def test_012_second_link_lands_on_instance_b(h2ac_admin, h2ac_admin_b):

  # Trigger from instance A.
  response = h2ac_admin.get(ADMIN_CLIENT_PROVISION_URI + "/multiEndpoint")
  assert response["status"] == 200

  time.sleep(2)

  # Instance A server-data must contain ONLY step-a.
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI)
  assert response["status"] == 200
  a_uris = {e["uri"] for e in response["body"]}
  assert STEP_A_URI in a_uris, "instance A must have received step-a"
  assert STEP_B_URI not in a_uris, (
    "instance A received step-b: second link misrouted to the inherited "
    "endpoint (pre-fix bug)"
  )

  # Instance B server-data must contain step-b (decisive post-fix assertion).
  response = h2ac_admin_b.get(ADMIN_SERVER_DATA_URI)
  assert response["status"] == 200, (
    "instance B has no server-data: the second link never physically reached "
    "endpoint epB (pre-fix bug)"
  )
  b_uris = {e["uri"] for e in response["body"]}
  assert STEP_B_URI in b_uris, "instance B must have physically received step-b"


@pytest.mark.skipif(not _two_instances_ready, reason=_skip_reason)
@pytest.mark.admin
def test_013_cleanup_two_instances(admin_cleanup, h2ac_admin_b):

  admin_cleanup()
  h2ac_admin_b.delete(ADMIN_SERVER_PROVISION_URI)
  h2ac_admin_b.delete(ADMIN_SERVER_DATA_URI)
