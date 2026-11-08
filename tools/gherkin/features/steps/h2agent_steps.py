"""
Step definitions of the h2agent Gherkin driver.

A provision is described by a starting step ("Given the server answers ...",
"Given the client ... sends ...") plus modifier steps ("And in state ...",
"And with transformations", ...). It is sent by the first step which does not
modify it, so there is no "commit" step. Vault entries and presets are sent at
once (they never send a pending provision first), so they are always loaded
before the provisions which reference them.

In dump mode the API calls are recorded instead of executed and the Then
steps are skipped.
"""

import json
import re

from behave import given, step, then, when, use_step_matcher

from h2agent_lib import (admin, eventually, flush_provision, json_or_text, json_pointer,
                         matches, parse_rules, pending, post_provision, preset_body, show,
                         start_provision, traffic)

use_step_matcher("re")

Q = r'"([^"]*)"'               # quoted text
D = r'(\d+)'                   # integer
QS = r'((?:"[^"]*"(?:, | and )?)+)'  # one or more quoted texts: "a", "b" and "c"
OPT_COLON = r':?'


def _names(quoted_list):
    return re.findall(r'"([^"]*)"', quoted_list)


def _doc(context):
    return json_or_text(context.text) if context.text else None


def _json_headers(headers, body):
    """A json body is sent / answered as json unless a content-type is given."""
    if isinstance(body, (dict, list)) and not any(k.lower() == "content-type" for k in headers):
        headers["content-type"] = "application/json"
    return headers


# ---------------------------------------------------------------------------
# PRE-PROVISION: vault entries and presets (sent at once, before provisions)
# ---------------------------------------------------------------------------

def _vault(context, key, value):
    admin(context, "vault", "POST", "vault", body={key: value})


@given(f'the vault entry {Q} is {Q}')
def step_vault_entry_text(context, key, value):
    _vault(context, key, value)


@given(f'the vault entry {Q}{OPT_COLON}')
def step_vault_entry_doc(context, key):
    assert context.text is not None, "the vault entry value goes in a doc string"
    _vault(context, key, _doc(context))


@given(f'the preset {Q}{OPT_COLON}')
def step_preset(context, name):
    value = _doc(context)
    assert isinstance(value, dict), f"preset '{name}': a json object in a doc string"
    _vault(context, name, value)


@given(f'vault {Q} with value {Q}')
def step_vault_set(context, key, value):
    _vault(context, key, value)


# ---------------------------------------------------------------------------
# SERVER MATCHING
# ---------------------------------------------------------------------------

def _matching(context, body):
    flush_provision(context)
    admin(context, "server-matching", "POST", "server-matching", body=body)


@given(f'server matching is {Q} with rgx {Q} and fmt {Q}')
def step_server_matching_regex(context, algorithm, rgx, fmt):
    _matching(context, {"algorithm": algorithm, "rgx": rgx, "fmt": fmt})


@given(f'server matching is {Q} with query parameters filter {Q} separator {Q}')
def step_server_matching_qp_sep(context, algorithm, qp_filter, sep):
    _matching(context, {"algorithm": algorithm,
                        "uriPathQueryParameters": {"filter": qp_filter, "separator": sep}})


@given(f'server matching is {Q} with query parameters filter {Q}')
def step_server_matching_qp(context, algorithm, qp_filter):
    _matching(context, {"algorithm": algorithm, "uriPathQueryParameters": {"filter": qp_filter}})


@given(f'server matching is {Q}')
def step_server_matching(context, algorithm):
    _matching(context, {"algorithm": algorithm})


# ---------------------------------------------------------------------------
# SERVER PROVISION
# ---------------------------------------------------------------------------

def _server(context, method, uri, code, body=None, presets=None):
    body = preset_body(body, presets)
    provision = {"requestMethod": method, "requestUri": uri, "responseCode": int(code)}
    if body is not None:
        provision["responseBody"] = body
        _json_headers(provision.setdefault("responseHeaders", {}), body)
    start_provision(context, "server-provision", provision)


@given(f'the server answers {Q} {Q} with {D} and body {Q}')
def step_server_answers_text(context, method, uri, code, text):
    _server(context, method, uri, code, json_or_text(text))


@given(f'the server answers {Q} {Q} with {D} from presets? {QS}{OPT_COLON}')
def step_server_answers_preset(context, method, uri, code, presets):
    _server(context, method, uri, code, _doc(context), _names(presets))


@given(f'the server answers {Q} {Q} with {D}{OPT_COLON}')
def step_server_answers(context, method, uri, code):
    _server(context, method, uri, code, _doc(context))


@step(f'with response header {Q} as {Q}')
def step_provision_response_header(context, name, value):
    pending(context, "server-provision").setdefault("responseHeaders", {})[name] = value


@step(f'with response delay {D} ms')
def step_provision_response_delay(context, ms):
    pending(context, "server-provision")["responseDelayMs"] = int(ms)


@step(f'with request schema {Q}')
def step_provision_request_schema(context, schema_id):
    pending(context)["requestSchemaId"] = schema_id


@step(f'with response schema {Q}')
def step_provision_response_schema(context, schema_id):
    pending(context)["responseSchemaId"] = schema_id


# Server and client provisions: states and transformations

@step(f'in state {Q} moving to {Q}')
def step_provision_states(context, in_state, out_state):
    pending(context).update({"inState": in_state, "outState": out_state})


@step(f'in state {Q}')
def step_provision_in_state(context, state):
    pending(context)["inState"] = state


@step(f'moving to state {Q}')
def step_provision_out_state(context, state):
    pending(context)["outState"] = state


@step(f'with transformations{OPT_COLON}')
def step_provision_transformations(context):
    assert context.table, "transformations go in a table: | from | filter | to |"
    pending(context).setdefault("transform", []).extend(parse_rules(context.table))


@step(f'with response transformations{OPT_COLON}')
def step_provision_response_transformations(context):
    assert context.table, "transformations go in a table: | from | filter | to |"
    pending(context, "client-provision").setdefault("onResponseTransform", []) \
        .extend(parse_rules(context.table))


# Json escape hatch: one rule (or a list) as in the REST API

@step('with transforms?')
def step_provision_transform_json(context):
    rules = json.loads(context.text)
    pending(context).setdefault("transform", []).extend(rules if isinstance(rules, list) else [rules])


@step('with on-response transforms?')
def step_provision_on_response_transform_json(context):
    rules = json.loads(context.text)
    pending(context, "client-provision").setdefault("onResponseTransform", []) \
        .extend(rules if isinstance(rules, list) else [rules])


@given('server provision')
def step_server_provision_json(context):
    flush_provision(context)
    post_provision(context, "server-provision", json.loads(context.text))


@given(f'server provision from file {Q}')
def step_server_provision_file(context, path):
    flush_provision(context)
    with open(path) as f:
        post_provision(context, "server-provision", json.load(f))


# Former builder steps (still accepted)

@given(f'a server provision for {Q} on {Q}')
def step_server_provision_start(context, method, uri):
    start_provision(context, "server-provision",
                    {"requestMethod": method, "requestUri": uri, "responseCode": 200})


@step(f'with inState {Q}')
def step_provision_in_state_old(context, state):
    pending(context)["inState"] = state


@step(f'with outState {Q}')
def step_provision_out_state_old(context, state):
    pending(context)["outState"] = state


@step(f'with response code {D}')
def step_provision_response_code(context, code):
    pending(context, "server-provision")["responseCode"] = int(code)


@step(f'with response body {Q}')
def step_provision_response_body_inline(context, text):
    pending(context, "server-provision")["responseBody"] = json_or_text(text)


@step('with response body')
def step_provision_response_body(context):
    pending(context, "server-provision")["responseBody"] = _doc(context)


@when(r'the (?:server|client) provision is committed')
def step_provision_commit(context):
    flush_provision(context)


# ---------------------------------------------------------------------------
# CLIENT ENDPOINT AND PROVISION
# ---------------------------------------------------------------------------

def _endpoint(context, ep_id, host, port, secure):
    flush_provision(context)
    body = {"id": ep_id, "host": host, "port": int(port)}
    if secure:
        body["secure"] = True
    admin(context, "client-endpoint", "POST", "client-endpoint", body=body,
          expected=(200, 201, 202))


@given(f'(?:the|a) secure client endpoint {Q} at {Q} port {D}')
def step_client_endpoint_secure(context, ep_id, host, port):
    _endpoint(context, ep_id, host, port, True)


@given(f'(?:the|a) client endpoint {Q} at {Q} port {D}')
def step_client_endpoint(context, ep_id, host, port):
    _endpoint(context, ep_id, host, port, False)


def _client(context, prov_id, method, uri, endpoint, body=None, presets=None):
    body = preset_body(body, presets)
    provision = {"id": prov_id, "endpoint": endpoint, "requestMethod": method, "requestUri": uri}
    if body is not None:
        provision["requestBody"] = body
        _json_headers(provision.setdefault("requestHeaders", {}), body)
    start_provision(context, "client-provision", provision)


@given(f'the client {Q} sends {Q} {Q} to {Q} from presets? {QS}{OPT_COLON}')
def step_client_sends_preset(context, prov_id, method, uri, endpoint, presets):
    _client(context, prov_id, method, uri, endpoint, _doc(context), _names(presets))


@given(f'the client {Q} sends {Q} {Q} to {Q}{OPT_COLON}')
def step_client_sends(context, prov_id, method, uri, endpoint):
    _client(context, prov_id, method, uri, endpoint, _doc(context))


@step(f'with request header {Q} as {Q}')
def step_provision_request_header(context, name, value):
    pending(context, "client-provision").setdefault("requestHeaders", {})[name] = value


@step(f'with request delay {D} ms')
def step_provision_request_delay(context, ms):
    pending(context, "client-provision")["requestDelayMs"] = int(ms)


@step(f'with timeout {D} ms')
def step_provision_timeout(context, ms):
    pending(context, "client-provision")["timeoutMs"] = int(ms)


@step(f'expecting status code {D}')
def step_provision_expected_status(context, code):
    pending(context, "client-provision")["expectedResponseStatusCode"] = int(code)


@given('client provision')
def step_client_provision_json(context):
    flush_provision(context)
    post_provision(context, "client-provision", json.loads(context.text))


# Former builder steps (still accepted)

@given(f'a client provision {Q}')
def step_client_provision_start(context, prov_id):
    start_provision(context, "client-provision", {"id": prov_id})


@step(f'with endpoint {Q}')
def step_provision_endpoint(context, ep_id):
    pending(context, "client-provision")["endpoint"] = ep_id


@step(f'with request method {Q}')
def step_provision_request_method(context, method):
    pending(context, "client-provision")["requestMethod"] = method


@step(f'with request uri {Q}')
def step_provision_request_uri(context, uri):
    pending(context, "client-provision")["requestUri"] = uri


@step('with request body')
def step_provision_request_body(context):
    pending(context, "client-provision")["requestBody"] = _doc(context)


# ---------------------------------------------------------------------------
# CLIENT TRIGGER (the trigger is accepted at once; the flow runs async)
# ---------------------------------------------------------------------------

def _trigger(context, prov_id, params=None):
    flush_provision(context)
    admin(context, f"trigger-{prov_id}", "POST", f"client-provision/{prov_id}/trigger",
          params=params, expected=(200, 202))


@when(f'client provision {Q} is triggered with sequence {D}')
def step_trigger_seq(context, prov_id, seq):
    _trigger(context, prov_id, {"sequence": int(seq)})


@when(f'client provision {Q} is triggered from {D} to {D} at {D} cps')
def step_trigger_range(context, prov_id, begin, end, cps):
    _trigger(context, prov_id, {"sequenceBegin": int(begin), "sequenceEnd": int(end),
                                "cps": int(cps)})


@when(f'client provision {Q} cps is set to {D}')
def step_trigger_cps(context, prov_id, cps):
    _trigger(context, prov_id, {"cps": int(cps)})


@when(f'(?:client provision|the client) {Q} is triggered')
def step_trigger(context, prov_id):
    _trigger(context, prov_id)


# ---------------------------------------------------------------------------
# TRAFFIC towards the mock server
# ---------------------------------------------------------------------------

def _send(context, method, uri, body=None, headers=None):
    flush_provision(context)
    traffic(context, method, uri, body, _json_headers(dict(headers or {}), body))


@when(f'I send {Q} to {Q} with body {Q}')
def step_send_body_inline(context, method, uri, text):
    _send(context, method, uri, json_or_text(text))


@when(f'I send {Q} to {Q} with header {Q} as {Q}')
def step_send_header(context, method, uri, name, value):
    _send(context, method, uri, headers={name: value})


@when(f'I send {Q} to {Q} with body{OPT_COLON}')
def step_send_body(context, method, uri):
    _send(context, method, uri, _doc(context))


@when(f'I send {Q} to {Q}')
def step_send(context, method, uri):
    _send(context, method, uri)


# ---------------------------------------------------------------------------
# ASSERTIONS (skipped in dump mode)
# ---------------------------------------------------------------------------

def _then(context):
    flush_provision(context)
    return not context.dump


def _response(context):
    assert context.last_response is not None, "no traffic response yet ('When I send ...')"
    return context.last_response


def _body_of(response):
    try:
        return response.json()
    except ValueError:
        return response.text


@then(f'the response code should be {D}')
def step_assert_code(context, code):
    if _then(context):
        r = _response(context)
        assert r.status_code == int(code), f"expected {code}, got {r.status_code}: {r.text}"


@then('the response body should be')
def step_assert_body_doc(context):
    if _then(context):
        actual = _body_of(_response(context))
        assert actual == _doc(context), f"body: {show(actual)}"


@then(f'the response body should be {Q}')
def step_assert_body_inline(context, expected):
    if _then(context):
        actual = _body_of(_response(context))
        assert matches(actual, expected), f"body: expected {expected}, got {show(actual)}"


@then(f'the response body at {Q} should be {Q}')
def step_assert_body_path(context, path, expected):
    if _then(context):
        actual = json_pointer(_body_of(_response(context)), path)
        assert matches(actual, expected), f"at {path}: expected {expected}, got {show(actual)}"


@then(f'the response header {Q} should be {Q}')
def step_assert_header(context, name, expected):
    if _then(context):
        actual = _response(context).headers.get(name)
        assert actual == expected, f"header '{name}': expected '{expected}', got '{actual}'"


# Server data

def _server_events(context, method, uri):
    r = context.h2.request("GET", context.h2.admin_url("server-data"),
                           params={"requestMethod": method, "requestUri": uri})
    if r.status_code == 204:
        return []
    assert r.status_code == 200, f"server-data query failed: {r.status_code} {r.text}"
    return r.json().get("events", [])


def _assert_received(context, method, uri, n):
    if _then(context):
        def check():
            count = len(_server_events(context, method, uri))
            assert count == n, f"{method} {uri}: expected {n} event(s), got {count}"
        eventually(context, check)


@then(f'the server should have received {D} {Q} {Q}')
def step_server_received(context, n, method, uri):
    _assert_received(context, method, uri, int(n))


@then(f'server data for {Q} {Q} should have {D} events?(?:\\(s\\))?')
def step_server_data_count(context, method, uri, n):
    _assert_received(context, method, uri, int(n))


def _server_event_at(context, method, uri, num, path):
    events = _server_events(context, method, uri)
    num = int(num)
    assert events and abs(num) <= len(events) and num != 0, \
        f"{method} {uri}: no event {num} ({len(events)} received)"
    return json_pointer(events[num - 1 if num > 0 else num], path)


@then(f'server data for {Q} {Q} event (-?\\d+) at {Q} should be {Q}')
def step_server_event_path(context, method, uri, num, path, expected):
    if _then(context):
        def check():
            actual = _server_event_at(context, method, uri, num, path)
            assert matches(actual, expected), f"at {path}: expected {expected}, got {show(actual)}"
        eventually(context, check)


@then(f'server data for {Q} {Q} event (-?\\d+) at {Q} should be')
def step_server_event_path_doc(context, method, uri, num, path):
    if _then(context):
        expected = _doc(context)

        def check():
            actual = _server_event_at(context, method, uri, num, path)
            assert actual == expected, f"at {path}: got {show(actual)}"
        eventually(context, check)


@when('server data is cleared')
def step_server_data_clear(context):
    flush_provision(context)
    admin(context, "clean-server-data", "DELETE", "server-data", expected=None)


@when(f'server data for {Q} {Q} is cleared')
def step_server_data_clear_filtered(context, method, uri):
    flush_provision(context)
    admin(context, "clean-server-data", "DELETE", "server-data",
          params={"requestMethod": method, "requestUri": uri}, expected=None)


# Client data

def _client_events(context, prov_id=None, params=None):
    r = context.h2.request("GET", context.h2.admin_url("client-data"), params=params)
    if r.status_code == 204:
        return []
    assert r.status_code == 200, f"client-data query failed: {r.status_code} {r.text}"
    data = r.json()
    items = data if isinstance(data, list) else [data]
    events = [e for item in items for e in item.get("events", [])]
    return [e for e in events if prov_id is None or e.get("clientProvisionId") == prov_id]


def _last_client_event(context, prov_id):
    events = [e for e in _client_events(context, prov_id) if e.get("responseStatusCode", -1) != -1]
    assert events, f"client '{prov_id}' has no response yet"
    return max(events, key=lambda e: e.get("sendseq", 0))


@then(f'the client {Q} should receive {D}')
def step_client_receives(context, prov_id, code):
    if _then(context):
        def check():
            actual = _last_client_event(context, prov_id)["responseStatusCode"]
            assert actual == int(code), f"client '{prov_id}': expected {code}, got {actual}"
        eventually(context, check)


@then(f'the client {Q} response body at {Q} should be {Q}')
def step_client_response_path(context, prov_id, path, expected):
    if _then(context):
        def check():
            body = _last_client_event(context, prov_id).get("responseBody")
            body = json_or_text(body) if isinstance(body, str) else body
            actual = json_pointer(body, path) if isinstance(body, (dict, list)) else body
            assert matches(actual, expected), f"at {path}: expected {expected}, got {show(actual)}"
        eventually(context, check)


@then(f'client data for {Q} {Q} {Q} should have {D} events?(?:\\(s\\))?')
def step_client_data_count(context, ep_id, method, uri, n):
    if _then(context):
        params = {"clientEndpointId": ep_id, "requestMethod": method, "requestUri": uri}

        def check():
            count = len([e for e in _client_events(context, params=params)
                         if e.get("responseStatusCode", -1) != -1])
            assert count == int(n), f"{ep_id} {method} {uri}: expected {n} event(s), got {count}"
        eventually(context, check)


@when('client data is cleared')
def step_client_data_clear(context):
    flush_provision(context)
    admin(context, "clean-client-data", "DELETE", "client-data", expected=None)


# ---------------------------------------------------------------------------
# SCHEMA
# ---------------------------------------------------------------------------

@given(f'(?:the|a) schema {Q}{OPT_COLON}')
def step_schema(context, schema_id):
    flush_provision(context)
    admin(context, "schema", "POST", "schema", body={"id": schema_id, "schema": _doc(context)})


# ---------------------------------------------------------------------------
# VAULT assertions and maintenance
# ---------------------------------------------------------------------------

def _vault_value(context, key):
    r = context.h2.request("GET", context.h2.admin_url("vault"), params={"name": key})
    assert r.status_code == 200, f"vault entry '{key}' not found ({r.status_code})"
    return r.json()


@then(f'(?:the )?vault(?: entry)? {Q} should be {Q}')
def step_vault_check(context, key, expected):
    if _then(context):
        def check():
            actual = _vault_value(context, key)
            assert matches(actual, expected), f"vault '{key}': expected {expected}, got {show(actual)}"
        eventually(context, check)


@then(f'(?:the )?vault(?: entry)? {Q} should be{OPT_COLON}')
def step_vault_check_doc(context, key):
    if _then(context):
        expected = _doc(context)

        def check():
            actual = _vault_value(context, key)
            assert actual == expected, f"vault '{key}': got {show(actual)}"
        eventually(context, check)


@when('vault is cleared')
def step_vault_clear(context):
    admin(context, "clean-vault", "DELETE", "vault", expected=None)


@when(f'vault {Q} is cleared')
def step_vault_clear_key(context, key):
    admin(context, f"clean-vault-{key}", "DELETE", "vault", params={"name": key}, expected=None)


@when(f'I wait for vault {Q} with value {Q} timeout {D} ms')
def step_wait_vault(context, key, value, ms):
    flush_provision(context)
    admin(context, f"wait-vault-{key}", "GET", f"vault/{key}/wait",
          params={"value": value, "timeoutMs": int(ms)}, expected=None)


@when(f'I wait for vault {Q} to change timeout {D} ms')
def step_wait_vault_any(context, key, ms):
    flush_provision(context)
    admin(context, f"wait-vault-{key}", "GET", f"vault/{key}/wait",
          params={"timeoutMs": int(ms)}, expected=None)


# ---------------------------------------------------------------------------
# LOGGING, CONFIGURATION, HEALTH
# ---------------------------------------------------------------------------

@given(f'logging level is {Q}')
def step_set_logging(context, level):
    flush_provision(context)
    admin(context, "logging", "PUT", "logging", params={"level": level})


@then(f'logging level should be {Q}')
def step_check_logging(context, level):
    if _then(context):
        r = context.h2.request("GET", context.h2.admin_url("logging"))
        assert r.text.strip() == level, f"expected '{level}', got '{r.text.strip()}'"


@given(r'server configuration receiveRequestBody is (true|false)')
def step_server_config(context, val):
    flush_provision(context)
    admin(context, "server-configuration", "PUT", "server/configuration",
          params={"receiveRequestBody": val})


@given(r'server data storage discard is (true|false) and discardKeyHistory is (true|false)')
def step_server_data_config(context, discard, dkh):
    flush_provision(context)
    admin(context, "server-data-configuration", "PUT", "server-data/configuration",
          params={"discard": discard, "discardKeyHistory": dkh})


@given(r'client data storage discard is (true|false) and discardKeyHistory is (true|false)')
def step_client_data_config(context, discard, dkh):
    flush_provision(context)
    admin(context, "client-data-configuration", "PUT", "client-data/configuration",
          params={"discard": discard, "discardKeyHistory": dkh})


@then('h2agent should be healthy')
def step_health(context):
    if _then(context):
        r = context.h2.request("GET", context.h2.admin_url("health"))
        assert r.status_code == 200, f"health check failed: {r.status_code}"


@then(f'there should be {D} unused server provisions?(?:\\(s\\))?')
def step_unused_server_provisions(context, n):
    if _then(context):
        r = context.h2.request("GET", context.h2.admin_url("server-provision/unused"))
        count = 0 if r.status_code == 204 else len(r.json() if isinstance(r.json(), list) else [r.json()])
        assert count == int(n), f"expected {n} unused provision(s), got {count}"


# ---------------------------------------------------------------------------
# GENERIC ADMIN (escape hatch): the response is kept as the last response
# ---------------------------------------------------------------------------

def _raw_admin(context, method, path, body=None):
    flush_provision(context)
    r = admin(context, path.split("?")[0].replace("/", "-"), method, path, body=body, expected=None)
    if r is not None:
        context.last_response = r


@when(f'I POST to admin {Q}')
def step_admin_post(context, path):
    _raw_admin(context, "POST", path, json.loads(context.text))


@when(f'I GET admin {Q}')
def step_admin_get(context, path):
    _raw_admin(context, "GET", path)


@when(f'I DELETE admin {Q}')
def step_admin_delete(context, path):
    _raw_admin(context, "DELETE", path)


@when(f'I PUT admin {Q}')
def step_admin_put(context, path):
    _raw_admin(context, "PUT", path, json.loads(context.text) if context.text else None)
