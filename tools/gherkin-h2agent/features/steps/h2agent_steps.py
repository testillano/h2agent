"""
Step definitions for h2agent Gherkin driver.

Covers the full admin REST API. Supports dump mode: when active,
API calls are recorded as numbered JSON files instead of executed.
"""

import json
from behave import given, when, then, step, use_step_matcher

use_step_matcher("re")

# Shorthand for quoted param capture
Q = r'"([^"]*)"'  # captures content between quotes
D = r'(\d+)'      # captures integer


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _parse_json(text):
    return json.loads(text.strip())


def _dump_or_exec(context, label, method, path, body=None, params=None,
                  expected_codes=(200, 201)):
    """In dump mode, record to file. Otherwise, execute against h2agent."""
    if context.dump:
        context.dump.record(label, method, path, body, params)
        return None

    url = context.h2.admin_url(path)
    if method == "POST":
        r = context.h2.post_json(url, body)
    elif method == "PUT":
        r = context.h2.put_json(url, body) if body else context.h2.put(url, params=params)
    elif method == "GET":
        r = context.h2.get(url, params=params)
    elif method == "DELETE":
        r = context.h2.delete(url, params=params)
    else:
        raise ValueError(f"Unknown method: {method}")

    context.last_response = r
    if expected_codes:
        assert r.status_code in expected_codes, \
            f"{label} failed: {r.status_code} {r.text}"
    return r


# ---------------------------------------------------------------------------
# SERVER MATCHING
# ---------------------------------------------------------------------------

@given(f'server matching is {Q} with rgx {Q} and fmt {Q}')
def step_server_matching_regex(context, algorithm, rgx, fmt):
    _dump_or_exec(context, "server-matching", "POST", "server-matching",
                  body={"algorithm": algorithm, "rgx": rgx, "fmt": fmt})


@given(f'server matching is {Q} with query parameters filter {Q} separator {Q}')
def step_server_matching_qp_sep(context, algorithm, qp_filter, sep):
    _dump_or_exec(context, "server-matching", "POST", "server-matching",
                  body={"algorithm": algorithm,
                        "uriPathQueryParameters": {"filter": qp_filter,
                                                   "separator": sep}})


@given(f'server matching is {Q} with query parameters filter {Q}')
def step_server_matching_qp(context, algorithm, qp_filter):
    _dump_or_exec(context, "server-matching", "POST", "server-matching",
                  body={"algorithm": algorithm,
                        "uriPathQueryParameters": {"filter": qp_filter}})


@given(f'server matching is {Q}')
def step_server_matching(context, algorithm):
    _dump_or_exec(context, "server-matching", "POST", "server-matching",
                  body={"algorithm": algorithm})


# ---------------------------------------------------------------------------
# SERVER PROVISION — builder pattern
# ---------------------------------------------------------------------------

@given(f'a server provision for {Q} on {Q}')
def step_server_provision_start(context, method, uri):
    context.provision = {"requestMethod": method, "requestUri": uri, "responseCode": 200}


@given(f'a server provision for {Q}')
def step_server_provision_start_no_uri(context, method):
    context.provision = {"requestMethod": method, "requestUri": "", "responseCode": 200}


@step(f'with inState {Q}')
def step_provision_in_state(context, state):
    context.provision["inState"] = state


@step(f'with outState {Q}')
def step_provision_out_state(context, state):
    context.provision["outState"] = state


@step(f'with response code {D}')
def step_provision_response_code(context, code):
    context.provision["responseCode"] = int(code)


@step(f'with response header {Q} as {Q}')
def step_provision_response_header(context, name, value):
    context.provision.setdefault("responseHeaders", {})[name] = value


@step('with response body')
def step_provision_response_body(context):
    context.provision["responseBody"] = _parse_json(context.text)


@step(f'with response body {Q}')
def step_provision_response_body_inline(context, text):
    try:
        context.provision["responseBody"] = json.loads(text)
    except json.JSONDecodeError:
        context.provision["responseBody"] = text


@step(f'with response delay {D} ms')
def step_provision_response_delay(context, ms):
    context.provision["responseDelayMs"] = int(ms)


@step(f'with request schema {Q}')
def step_provision_request_schema(context, schema_id):
    context.provision["requestSchemaId"] = schema_id


@step(f'with response schema {Q}')
def step_provision_response_schema(context, schema_id):
    context.provision["responseSchemaId"] = schema_id


@step(f'with transform from {Q} to {Q} and filter {Q}')
def step_provision_transform_string_filter(context, source, target, filt):
    item = {"source": source, "target": target, "filter": filt}
    context.provision.setdefault("transform", []).append(item)


@step(f'with transform from {Q} to {Q} and filter')
def step_provision_transform_filter(context, source, target):
    item = {"source": source, "target": target, "filter": _parse_json(context.text)}
    context.provision.setdefault("transform", []).append(item)


@step(f'with transform from {Q} to {Q}')
def step_provision_transform(context, source, target):
    context.provision.setdefault("transform", []).append(
        {"source": source, "target": target}
    )


@step('with transforms')
def step_provision_transforms_json(context):
    context.provision["transform"] = _parse_json(context.text)


@when('the server provision is committed')
def step_server_provision_commit(context):
    _dump_or_exec(context, "server-provision", "POST", "server-provision",
                  body=context.provision)
    context.provision = {}


# ---------------------------------------------------------------------------
# SERVER PROVISION — one-shot JSON
# ---------------------------------------------------------------------------

@given('server provision')
def step_server_provision_json(context):
    _dump_or_exec(context, "server-provision", "POST", "server-provision",
                  body=_parse_json(context.text))


@given(f'server provision from file {Q}')
def step_server_provision_file(context, path):
    with open(path) as f:
        body = json.load(f)
    _dump_or_exec(context, "server-provision", "POST", "server-provision",
                  body=body)


# ---------------------------------------------------------------------------
# SERVER DATA
# ---------------------------------------------------------------------------

@then(f'server data for {Q} {Q} should have {D} event\\(s\\)')
def step_server_data_count(context, method, uri, n):
    n = int(n)
    if context.dump:
        return
    r = context.h2.get(
        context.h2.admin_url("server-data"),
        params={"requestMethod": method, "requestUri": uri}
    )
    if n == 0:
        assert r.status_code == 204, f"Expected no data, got {r.status_code}"
        return
    assert r.status_code == 200, f"server-data query failed: {r.status_code}"
    data = r.json()
    events = data.get("events", data) if isinstance(data, dict) else data
    count = len(events.get("events", [])) if isinstance(events, dict) else len(events) if isinstance(events, list) else 0
    assert count == n, f"Expected {n} events, got {count}"


@then(f'server data for {Q} {Q} event (-?\\d+) at {Q} should be {Q}')
def step_server_data_event_path(context, method, uri, num, path, expected):
    if context.dump:
        return
    r = context.h2.get(
        context.h2.admin_url("server-data"),
        params={"requestMethod": method, "requestUri": uri,
                "eventNumber": int(num), "eventPath": path}
    )
    assert r.status_code == 200, f"server-data query failed: {r.status_code}"
    actual = r.text.strip().strip('"')
    assert actual == expected, f"Expected '{expected}', got '{actual}'"


@then(f'server data for {Q} {Q} event (-?\\d+) at {Q} should be')
def step_server_data_event_path_json(context, method, uri, num, path):
    if context.dump:
        return
    r = context.h2.get(
        context.h2.admin_url("server-data"),
        params={"requestMethod": method, "requestUri": uri,
                "eventNumber": int(num), "eventPath": path}
    )
    assert r.status_code == 200, f"server-data query failed: {r.status_code}"
    assert r.json() == _parse_json(context.text)


@when('server data is cleared')
def step_server_data_clear(context):
    _dump_or_exec(context, "clean-server-data", "DELETE", "server-data",
                  expected_codes=None)


@when(f'server data for {Q} {Q} is cleared')
def step_server_data_clear_filtered(context, method, uri):
    _dump_or_exec(context, "clean-server-data", "DELETE", "server-data",
                  params={"requestMethod": method, "requestUri": uri},
                  expected_codes=None)


# ---------------------------------------------------------------------------
# CLIENT ENDPOINT
# ---------------------------------------------------------------------------

@given(f'a secure client endpoint {Q} at {Q} port {D}')
def step_client_endpoint_secure(context, ep_id, host, port):
    _dump_or_exec(context, "client-endpoint", "POST", "client-endpoint",
                  body={"id": ep_id, "host": host, "port": int(port), "secure": True},
                  expected_codes=(200, 201, 202))


@given(f'a client endpoint {Q} at {Q} port {D}')
def step_client_endpoint(context, ep_id, host, port):
    _dump_or_exec(context, "client-endpoint", "POST", "client-endpoint",
                  body={"id": ep_id, "host": host, "port": int(port)},
                  expected_codes=(200, 201, 202))


# ---------------------------------------------------------------------------
# CLIENT PROVISION — builder pattern
# ---------------------------------------------------------------------------

@given(f'a client provision {Q}')
def step_client_provision_start(context, prov_id):
    context.provision = {"id": prov_id}


@step(f'with endpoint {Q}')
def step_provision_endpoint(context, ep_id):
    context.provision["endpoint"] = ep_id


@step(f'with request method {Q}')
def step_provision_request_method(context, method):
    context.provision["requestMethod"] = method


@step(f'with request uri {Q}')
def step_provision_request_uri(context, uri):
    context.provision["requestUri"] = uri


@step(f'with request header {Q} as {Q}')
def step_provision_request_header(context, name, value):
    context.provision.setdefault("requestHeaders", {})[name] = value


@step('with request body')
def step_provision_request_body(context):
    context.provision["requestBody"] = _parse_json(context.text)


@step(f'with request delay {D} ms')
def step_provision_request_delay(context, ms):
    context.provision["requestDelayMs"] = int(ms)


@step(f'with timeout {D} ms')
def step_provision_timeout(context, ms):
    context.provision["timeoutMs"] = int(ms)


@step(f'with on-response transform from {Q} to {Q} and filter {Q}')
def step_provision_on_response_transform_string_filter(context, source, target, filt):
    item = {"source": source, "target": target, "filter": filt}
    context.provision.setdefault("onResponseTransform", []).append(item)


@step(f'with on-response transform from {Q} to {Q} and filter')
def step_provision_on_response_transform_filter(context, source, target):
    item = {"source": source, "target": target, "filter": _parse_json(context.text)}
    context.provision.setdefault("onResponseTransform", []).append(item)


@step(f'with on-response transform from {Q} to {Q}')
def step_provision_on_response_transform(context, source, target):
    context.provision.setdefault("onResponseTransform", []).append(
        {"source": source, "target": target}
    )


@step('with on-response transforms')
def step_provision_on_response_transforms_json(context):
    context.provision["onResponseTransform"] = _parse_json(context.text)


@when('the client provision is committed')
def step_client_provision_commit(context):
    _dump_or_exec(context, "client-provision", "POST", "client-provision",
                  body=context.provision)
    context.provision = {}


@given('client provision')
def step_client_provision_json(context):
    _dump_or_exec(context, "client-provision", "POST", "client-provision",
                  body=_parse_json(context.text))


# ---------------------------------------------------------------------------
# CLIENT PROVISION — trigger
# ---------------------------------------------------------------------------

@when(f'client provision {Q} is triggered with sequence {D}')
def step_trigger_client_provision_seq(context, prov_id, seq):
    _dump_or_exec(context, f"trigger-{prov_id}", "GET",
                  f"client-provision/{prov_id}",
                  params={"sequence": int(seq)}, expected_codes=None)


@when(f'client provision {Q} is triggered from {D} to {D} at {D} cps')
def step_trigger_client_provision_range(context, prov_id, begin, end, cps):
    _dump_or_exec(context, f"trigger-{prov_id}", "GET",
                  f"client-provision/{prov_id}",
                  params={"sequenceBegin": int(begin), "sequenceEnd": int(end),
                          "cps": int(cps)},
                  expected_codes=None)


@when(f'client provision {Q} cps is set to {D}')
def step_trigger_client_provision_cps_only(context, prov_id, cps):
    _dump_or_exec(context, f"trigger-{prov_id}", "GET",
                  f"client-provision/{prov_id}",
                  params={"cps": int(cps)}, expected_codes=None)


@when(f'client provision {Q} is triggered')
def step_trigger_client_provision(context, prov_id):
    _dump_or_exec(context, f"trigger-{prov_id}", "GET",
                  f"client-provision/{prov_id}", expected_codes=None)


# ---------------------------------------------------------------------------
# CLIENT DATA
# ---------------------------------------------------------------------------

@then(f'client data for {Q} {Q} {Q} should have {D} event\\(s\\)')
def step_client_data_count(context, ep_id, method, uri, n):
    n = int(n)
    if context.dump:
        return
    r = context.h2.get(
        context.h2.admin_url("client-data"),
        params={"clientEndpointId": ep_id, "requestMethod": method, "requestUri": uri}
    )
    if n == 0:
        assert r.status_code == 204, f"Expected no data, got {r.status_code}"
        return
    assert r.status_code == 200, f"client-data query failed: {r.status_code}"
    data = r.json()
    events = data.get("events", []) if isinstance(data, dict) else data
    assert len(events) == n, f"Expected {n} events, got {len(events)}"


@when('client data is cleared')
def step_client_data_clear(context):
    _dump_or_exec(context, "clean-client-data", "DELETE", "client-data",
                  expected_codes=None)


# ---------------------------------------------------------------------------
# TRAFFIC — direct HTTP to traffic port
# ---------------------------------------------------------------------------

@when(f'I send {Q} to {Q} with body {Q}')
def step_send_traffic_body_inline(context, method, uri, text):
    if context.dump:
        try:
            body = json.loads(text)
        except json.JSONDecodeError:
            body = text
        context.dump.record(f"traffic-{method}", method, uri, body=body, is_traffic=True)
        return
    url = context.h2.traffic_url(uri)
    try:
        body = json.loads(text)
        context.last_response = context.h2.session.request(method, url, json=body)
    except json.JSONDecodeError:
        context.last_response = context.h2.session.request(method, url, data=text)


@when(f'I send {Q} to {Q} with header {Q} as {Q}')
def step_send_traffic_header(context, method, uri, hname, hval):
    if context.dump:
        context.dump.record(f"traffic-{method}", method, uri,
                            body={"_headers": {hname: hval}}, is_traffic=True)
        return
    context.last_response = context.h2.session.request(
        method, context.h2.traffic_url(uri), headers={hname: hval}
    )


@when(f'I send {Q} to {Q} with body')
def step_send_traffic_body(context, method, uri):
    body_text = context.text.strip()
    if context.dump:
        try:
            body = _parse_json(body_text)
        except json.JSONDecodeError:
            body = body_text
        context.dump.record(f"traffic-{method}", method, uri, body=body, is_traffic=True)
        return
    url = context.h2.traffic_url(uri)
    try:
        body = _parse_json(body_text)
        context.last_response = context.h2.session.request(method, url, json=body)
    except json.JSONDecodeError:
        context.last_response = context.h2.session.request(method, url, data=body_text)


@when(f'I send {Q} to {Q}')
def step_send_traffic(context, method, uri):
    if context.dump:
        context.dump.record(f"traffic-{method}", method, uri, is_traffic=True)
        return
    context.last_response = context.h2.session.request(method, context.h2.traffic_url(uri))


# ---------------------------------------------------------------------------
# RESPONSE ASSERTIONS (skipped in dump mode)
# ---------------------------------------------------------------------------

@then(f'the response code should be {D}')
def step_assert_response_code(context, code):
    if context.dump:
        return
    assert context.last_response is not None, "No response captured"
    assert context.last_response.status_code == int(code), \
        f"Expected {code}, got {context.last_response.status_code}: {context.last_response.text}"


@then('the response body should be')
def step_assert_response_body_json(context):
    if context.dump:
        return
    assert context.last_response.json() == _parse_json(context.text), \
        f"Body mismatch: {context.last_response.text}"


@then(f'the response body should be {Q}')
def step_assert_response_body_text(context, text):
    if context.dump:
        return
    actual = context.last_response.text.strip()
    assert actual == text, f"Expected '{text}', got '{actual}'"


@then(f'the response body at {Q} should be {Q}')
def step_assert_response_body_path(context, path, expected):
    if context.dump:
        return
    body = context.last_response.json()
    node = body
    for k in (k for k in path.strip("/").split("/") if k):
        node = node[int(k)] if isinstance(node, list) else node[k]
    assert str(node) == expected, f"At {path}: expected '{expected}', got '{node}'"


@then(f'the response header {Q} should be {Q}')
def step_assert_response_header(context, name, expected):
    if context.dump:
        return
    actual = context.last_response.headers.get(name)
    assert actual == expected, f"Header '{name}': expected '{expected}', got '{actual}'"


@then(r'the response result should be (true|false)')
def step_assert_result(context, result):
    if context.dump:
        return
    body = context.last_response.json()
    expected = result == "true"
    assert body.get("result") == expected, f"Expected result={expected}, got {body}"


# ---------------------------------------------------------------------------
# SCHEMA
# ---------------------------------------------------------------------------

@given(f'a schema {Q}')
def step_load_schema(context, schema_id):
    _dump_or_exec(context, "schema", "POST", "schema",
                  body={"id": schema_id, "schema": _parse_json(context.text)})


# ---------------------------------------------------------------------------
# VAULT
# ---------------------------------------------------------------------------

@given(f'vault {Q} with value {Q}')
def step_vault_set(context, key, value):
    _dump_or_exec(context, "vault", "POST", "vault", body={key: value})


@then(f'vault {Q} should be {Q}')
def step_vault_check(context, key, expected):
    if context.dump:
        return
    r = context.h2.get(context.h2.admin_url("vault"), params={"name": key})
    assert r.status_code == 200, f"vault get failed: {r.status_code}"
    assert r.text.strip() == expected, f"Vault '{key}': expected '{expected}', got '{r.text.strip()}'"


@then(f'vault {Q} should be')
def step_vault_check_json(context, key):
    if context.dump:
        return
    r = context.h2.get(context.h2.admin_url("vault"), params={"name": key})
    assert r.status_code == 200, f"vault get failed: {r.status_code}"
    assert r.json() == _parse_json(context.text), \
        f"Vault '{key}': expected {context.text.strip()}, got {r.text.strip()}"


@when('vault is cleared')
def step_vault_clear(context):
    _dump_or_exec(context, "clean-vault", "DELETE", "vault",
                  expected_codes=None)


@when(f'vault {Q} is cleared')
def step_vault_clear_key(context, key):
    _dump_or_exec(context, f"clean-vault-{key}", "DELETE", "vault",
                  params={"name": key}, expected_codes=None)


# ---------------------------------------------------------------------------
# WAIT (long-poll)
# ---------------------------------------------------------------------------

@when(f'I wait for vault {Q} with value {Q} timeout {D} ms')
def step_wait_vault(context, key, value, ms):
    _dump_or_exec(context, f"wait-vault-{key}", "GET", f"vault/{key}/wait",
                  params={"value": value, "timeoutMs": int(ms)},
                  expected_codes=None)


@when(f'I wait for vault {Q} to change timeout {D} ms')
def step_wait_vault_any(context, key, ms):
    _dump_or_exec(context, f"wait-vault-{key}", "GET", f"vault/{key}/wait",
                  params={"timeoutMs": int(ms)}, expected_codes=None)


# ---------------------------------------------------------------------------
# LOGGING
# ---------------------------------------------------------------------------

@given(f'logging level is {Q}')
def step_set_logging(context, level):
    _dump_or_exec(context, "logging", "PUT", "logging",
                  params={"level": level})


@then(f'logging level should be {Q}')
def step_check_logging(context, level):
    if context.dump:
        return
    r = context.h2.get(context.h2.admin_url("logging"))
    assert r.status_code == 200
    assert r.text.strip() == level, f"Expected '{level}', got '{r.text.strip()}'"


# ---------------------------------------------------------------------------
# CONFIGURATION
# ---------------------------------------------------------------------------

@given(r'server configuration receiveRequestBody is (true|false)')
def step_server_config(context, val):
    _dump_or_exec(context, "server-configuration", "PUT",
                  "server/configuration",
                  params={"receiveRequestBody": val})


@given(r'server data storage discard is (true|false) and discardKeyHistory is (true|false)')
def step_server_data_config(context, discard, dkh):
    _dump_or_exec(context, "server-data-configuration", "PUT",
                  "server-data/configuration",
                  params={"discard": discard, "discardKeyHistory": dkh})


@given(r'client data storage discard is (true|false) and discardKeyHistory is (true|false)')
def step_client_data_config(context, discard, dkh):
    _dump_or_exec(context, "client-data-configuration", "PUT",
                  "client-data/configuration",
                  params={"discard": discard, "discardKeyHistory": dkh})


# ---------------------------------------------------------------------------
# HEALTH & UNUSED
# ---------------------------------------------------------------------------

@then('h2agent should be healthy')
def step_health(context):
    if context.dump:
        return
    r = context.h2.get(context.h2.admin_url("health"))
    assert r.status_code == 200, f"health check failed: {r.status_code}"


@then(f'there should be {D} unused server provision\\(s\\)')
def step_unused_server_provisions(context, n):
    n = int(n)
    if context.dump:
        return
    r = context.h2.get(context.h2.admin_url("server-provision/unused"))
    if n == 0:
        assert r.status_code == 204, f"Expected no unused provisions, got {r.status_code}"
        return
    assert r.status_code == 200
    data = r.json()
    items = data if isinstance(data, list) else [data]
    assert len(items) == n, f"Expected {n} unused provisions, got {len(items)}"


# ---------------------------------------------------------------------------
# GENERIC ADMIN — full escape hatch
# ---------------------------------------------------------------------------

@when(f'I POST to admin {Q}')
def step_admin_post(context, path):
    _dump_or_exec(context, path.split("?")[0].replace("/", "-"), "POST",
                  path, body=_parse_json(context.text), expected_codes=None)


@when(f'I GET admin {Q}')
def step_admin_get(context, path):
    _dump_or_exec(context, path.split("?")[0].replace("/", "-"), "GET",
                  path, expected_codes=None)


@when(f'I DELETE admin {Q}')
def step_admin_delete(context, path):
    _dump_or_exec(context, path.split("?")[0].replace("/", "-"), "DELETE",
                  path, expected_codes=None)


@when(f'I PUT admin {Q}')
def step_admin_put(context, path):
    body = _parse_json(context.text) if context.text else None
    _dump_or_exec(context, path.split("?")[0].replace("/", "-"), "PUT",
                  path, body=body, expected_codes=None)
