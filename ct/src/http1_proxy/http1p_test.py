import json
import os
import socket
import time
import http.client
import pytest
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI


# HTTP/1 access through the nghttpx proxy of the image (built from the pinned
# nghttp2). A proxy rejection never reaches h2agent, so the stored event (not
# only the status code) proves the path.

H2AGENT_HOST = os.environ.get('H2AGENT_SERVICE_HOST', 'h2agent')
TRAFFIC_PROXY_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_TRAFFIC_PROXY', 8001))
ADMIN_PROXY_PORT = int(os.environ.get('H2AGENT_SERVICE_PORT_ADMIN_PROXY', 8076))

URI = "/app/v1/http1-proxy/alarm"
BODY = '{"alarmName":"foo","severity":"major"}'


def http1(method, uri, body = None, headers = None, port = TRAFFIC_PROXY_PORT):
  headers = headers or {}
  connection = http.client.HTTPConnection(H2AGENT_HOST, port, timeout = 5)
  # with an explicit 'Transfer-Encoding: chunked' header, http.client sends the body
  # raw unless told to frame it (encode_chunked)
  chunked = headers.get("Transfer-Encoding", "").lower() == "chunked"
  connection.request(method, uri, body = body, headers = headers, encode_chunked = chunked)
  response = connection.getresponse()
  status, payload = response.status, response.read()
  connection.close()
  return status, payload


def raw_upgrade(method, uri, body = ""):
  # 'Upgrade: h2c' as libcurl sends it (CURL_HTTP_VERSION_2 on http): http.client
  # does not handle the '101', so the request is written by hand and only the
  # first status line is read (101 when upgraded, else the HTTP/1.1 answer).
  request = (f"{method} {uri} HTTP/1.1\r\nHost: {H2AGENT_HOST}\r\n"
             "Connection: Upgrade, HTTP2-Settings\r\nUpgrade: h2c\r\nHTTP2-Settings: AAMAAABkAARAAAAAAAIAAAAA\r\n")
  if body:
    request += f"Content-Type: application/json\r\nContent-Length: {len(body)}\r\n"
  request += "\r\n" + body
  with socket.create_connection((H2AGENT_HOST, TRAFFIC_PROXY_PORT), timeout = 5) as s:
    s.sendall(request.encode())
    data = b""
    while b"\r\n" not in data:
      chunk = s.recv(4096)
      if not chunk: break
      data += chunk
  return int(data.split(b" ")[1])


def wait_events(h2ac_admin, method, expected, timeout = 3):
  # the event is stored once h2agent answers (after the '101' on an upgrade)
  deadline = time.time() + timeout
  while stored_events(h2ac_admin, method) < expected and time.time() < deadline: time.sleep(0.1)
  return stored_events(h2ac_admin, method)


def stored_events(h2ac_admin, method):
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=" + method + "&requestUri=" + URI)
  if response["status"] != 200: return 0
  return len(response["body"]["events"])


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup()


@pytest.mark.admin
def test_001_provision(h2ac_admin):

  provisions = [
    {"requestMethod": "POST", "requestUri": URI, "responseCode": 204},
    {"requestMethod": "GET", "requestUri": URI, "responseCode": 200, "responseBody": {"ok": True}}
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provisions)
  assert response["status"] == 201


@pytest.mark.server
@pytest.mark.parametrize("headers, body", [
  ({"Content-Type": "application/json"}, BODY),                                   # Content-Length
  ({"Content-Type": "application/json", "Transfer-Encoding": "chunked"}, iter([BODY.encode()])),
  ({"Content-Type": "application/json", "Expect": "100-continue"}, BODY),
], ids = ["content-length", "chunked", "expect-100-continue"])
def test_002_post_http1(h2ac_admin, headers, body):

  before = stored_events(h2ac_admin, "POST")
  status, _ = http1("POST", URI, body, headers)
  assert status == 204
  assert stored_events(h2ac_admin, "POST") == before + 1


@pytest.mark.server
@pytest.mark.parametrize("method, body", [("POST", BODY), ("GET", "")], ids = ["post-with-body", "get"])
def test_003_upgrade_h2c(h2ac_admin, method, body):

  # upgraded (101) or served over HTTP/1.1, never rejected by the proxy (400)
  before = stored_events(h2ac_admin, method)
  status = raw_upgrade(method, URI, body)
  assert status in (101, 200, 204)
  assert wait_events(h2ac_admin, method, before + 1) == before + 1


@pytest.mark.admin
def test_004_admin_through_proxy():

  status, payload = http1("GET", "/admin/v2/server-provision", port = ADMIN_PROXY_PORT)
  assert status == 200
  assert any(p["requestUri"] == URI for p in json.loads(payload))
