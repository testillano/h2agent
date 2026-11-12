import pytest
from urllib.parse import quote
from conftest import ADMIN_SERVER_PROVISION_URI, ADMIN_SERVER_DATA_URI


# The server uri is the received one, byte by byte (no path decoding): matching,
# event keys, sources, server-data queries and waits all use it. A uri given as an
# admin query value is encoded once more, as any query value.

BASKET = "/app/v1/received-uri/baskets/alice%3Bapples%3B42"      # sent by the client
DOUBLE = "/app/v1/received-uri/receipts/bob%253Bpears%253B7"             # double encoding (sent as is)
ITEMS = "/app/v1/received-uri/items"


def qv(value):
  # admin query value: encoded once more ('%' -> '%25', '/' kept for readability)
  return quote(value, safe = "/")


@pytest.mark.admin
def test_000_cleanup(admin_cleanup):

  admin_cleanup(matchingContent = {"algorithm": "RegexMatching"})


@pytest.mark.admin
def test_001_provision(h2ac_admin):

  provisions = [
    {"requestMethod": "DELETE", "requestUri": "/app/v1/received-uri/baskets/alice%3B[a-z]+%3B[0-9]+", "responseCode": 200,
     "outState": "termination", "responseHeaders": {"content-type": "application/json"}, "responseBody": {},
     "transform": [
       {"from": {"source": "requestUriPath"}, "to": {"target": "responseBody", "path": "/path"}},
       {"from": {"source": "requestUriPath"}, "filter": [{"op": "regexReplace", "rgx": ".*/baskets/([^/]+)", "fmt": "$1"}, {"op": "urlDecode"}],
        "to": {"target": "responseBody", "path": "/basketId"}}
     ]},
    {"requestMethod": "POST", "requestUri": "/app/v1/received-uri/receipts/bob%253B[a-z]+%253B[0-9]+", "responseCode": 201},
    {"requestMethod": "GET", "requestUri": "/app/v1/received-uri/items/[^/]+", "responseCode": 200, "outState": "one-segment"},
    {"requestMethod": "GET", "requestUri": "/app/v1/received-uri/items/[^/]+/[^/]+", "responseCode": 200, "outState": "two-segments"}
  ]
  response = h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, provisions)
  assert response["status"] == 201


@pytest.mark.server
def test_002_matching_and_sources_use_the_received_path(h2ac_admin, h2ac_traffic):

  response = h2ac_traffic.delete(BASKET)
  assert response["status"] == 200
  assert response["body"] == {"path": BASKET, "basketId": "alice;apples;42"}

  # the decoded form is another uri (not provisioned):
  response = h2ac_traffic.delete("/app/v1/received-uri/baskets/alice;apples;42")
  assert response["status"] == 501

  response = h2ac_traffic.postDict(DOUBLE)
  assert response["status"] == 201


@pytest.mark.server
def test_003_encoded_slash_is_not_a_segment(h2ac_admin, h2ac_traffic):

  assert h2ac_traffic.get(ITEMS + "/a%2Fb")["status"] == 200
  assert h2ac_traffic.get(ITEMS + "/a/b")["status"] == 200

  # two keys, each one with its own state:
  for uri, state in ((ITEMS + "/a%2Fb", "one-segment"), (ITEMS + "/a/b", "two-segments")):
    response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=GET&requestUri=" + qv(uri) + "&eventNumber=-1")
    assert response["status"] == 200
    assert response["body"]["state"] == state


@pytest.mark.server
def test_004_server_data_and_wait_with_the_received_uri(h2ac_admin):

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "?requestMethod=DELETE&requestUri=" + qv(BASKET) + "&eventNumber=1")
  assert response["status"] == 200
  assert response["body"]["state"] == "termination"

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=DELETE&requestUri=" + qv(BASKET) + "&state=termination&timeoutMs=2000")
  assert response["status"] == 200
  assert response["body"]["key"] == {"requestMethod": "DELETE", "requestUri": BASKET}

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=POST&requestUri=" + qv(DOUBLE) + "&eventNumber=1&timeoutMs=2000")
  assert response["status"] == 200

  # the decoded form is not the key:
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=DELETE&requestUri=" + qv("/app/v1/received-uri/baskets/alice;apples;42") + "&state=termination&timeoutMs=200")
  assert response["status"] == 408

  # uriRegex against the received uri:
  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/wait?requestMethod=DELETE&uriRegex=" + qv("%3Bapples%3B42$") + "&state=termination&timeoutMs=2000")
  assert response["status"] == 200


@pytest.mark.server
def test_005_sequence_shows_the_received_uri(h2ac_admin):

  response = h2ac_admin.get(ADMIN_SERVER_DATA_URI + "/sequence?requestMethod=DELETE")
  assert response["status"] == 200
  uris = [e["uri"] for e in response["body"] if e["direction"] == "recv"]
  assert BASKET in uris
