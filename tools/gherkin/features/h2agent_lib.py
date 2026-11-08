"""
Shared logic of the h2agent Gherkin driver (plain module: imported by the
behave hooks and the step definitions, and testable without behave).
"""

import json
import os
import re
import sys
import time

import httpx


# ---------------------------------------------------------------------------
# Transport and dump mode
# ---------------------------------------------------------------------------

class H2AgentClient:
    """h2agent admin and traffic interfaces: HTTP/2 with prior knowledge."""

    def __init__(self, admin_endpoint, traffic_endpoint):
        self.admin = admin_endpoint.rstrip("/")
        self.traffic = traffic_endpoint.rstrip("/")
        # h2agent only speaks HTTP/2 (no HTTP/1.1 upgrade): prior knowledge
        self.session = httpx.Client(http1=False, http2=True, timeout=10.0)

    def admin_url(self, path):
        return f"{self.admin}/admin/v2/{path.lstrip('/')}"

    def traffic_url(self, path):
        return f"{self.traffic}/{path.lstrip('/')}"

    def request(self, method, url, body=None, params=None, headers=None):
        kwargs = {"params": params, "headers": dict(headers or {})}
        if isinstance(body, (dict, list)):
            kwargs["json"] = body
        elif body is not None:
            kwargs["content"] = str(body)
        return self.session.request(method, url, **kwargs)

    def clean_all(self):
        for resource in ("server-provision", "server-data",
                         "client-provision", "client-endpoint", "client-data",
                         "schema", "vault"):
            self.request("DELETE", self.admin_url(resource))


class DumpRecorder:
    """Records API calls as numbered JSON files instead of executing them."""

    def __init__(self, output_dir):
        if os.path.exists(output_dir):
            print(f"ERROR: dump directory already exists: {output_dir}", file=sys.stderr)
            sys.exit(1)
        self.output_dir = output_dir
        self.seq = 0
        os.makedirs(output_dir)

    def record(self, label, method, path, body=None, params=None, headers=None,
               is_traffic=False):
        self.seq += 1
        safe_label = label.replace("/", "_").replace(" ", "-")
        entry = {"method": method}
        if is_traffic:
            entry["path"] = path
            entry["type"] = "traffic"
        else:
            entry["path"] = f"/admin/v2/{path}"
        if params:
            entry["queryParams"] = params
        if headers:
            entry["headers"] = headers
        if body is not None:
            entry["body"] = body
        with open(os.path.join(self.output_dir, f"{self.seq:02d}.{safe_label}.json"), "w") as f:
            json.dump(entry, f, indent=2, ensure_ascii=False)
            f.write("\n")


def admin(context, label, method, path, body=None, params=None, expected=(200, 201)):
    """Admin call (recorded in dump mode). Fails on an unexpected status code."""
    if context.dump:
        context.dump.record(label, method, path, body, params)
        return None
    r = context.h2.request(method, context.h2.admin_url(path), body=body, params=params)
    if expected:
        assert r.status_code in expected, f"{label} failed: {r.status_code} {r.text}"
    return r


def traffic(context, method, uri, body=None, headers=None):
    """Traffic request towards the agent's mock server; keeps its response."""
    if context.dump:
        context.dump.record(f"traffic-{method}", method, uri, body=body, headers=headers,
                            is_traffic=True)
        return
    context.last_response = context.h2.request(method, context.h2.traffic_url(uri),
                                               body=body, headers=headers)


# ---------------------------------------------------------------------------
# Provision being built: sent by the first step which does not modify it
# ---------------------------------------------------------------------------

def start_provision(context, kind, body):
    flush_provision(context)
    context.pending = (kind, body)


def pending(context, kind=None):
    assert context.pending, "no provision is being described in this step's context"
    if kind:
        wanted, current = kind.split("-")[0], context.pending[0].split("-")[0]
        assert wanted == current, f"this step applies to a {wanted} provision, not a {current} one"
    return context.pending[1]


def describe(kind, body):
    if "id" in body:
        return f"client provision '{body['id']}' ({body.get('inState', 'initial')})"
    return (f"server provision {body.get('requestMethod')} {body.get('requestUri')}"
            f" ({body.get('inState', 'initial')})")


def flush_provision(context):
    if not getattr(context, "pending", None):
        return
    kind, body = context.pending
    context.pending = None
    post_provision(context, kind, body)


def post_provision(context, kind, body):
    """POST a provision; a reference left unresolved (missing vault entry or
    preset at load time) is an error, not a silent literal."""
    what = describe(kind, body) if isinstance(body, dict) else kind
    r = admin(context, kind, "POST", kind, body=body, expected=None)
    if r is not None:
        assert r.status_code == 201, f"{what} rejected: {r.status_code} {r.text}"
        warning = _json_or_none(r.text) or {}
        warning = warning.get("warning") if isinstance(warning, dict) else None
        assert not warning, (f"{what}: {warning}. Vault entries and presets are loaded"
                             " when the provision is: declare them before (e.g. in Background)")


def preset_body(body, presets):
    """Start an object body from vault presets ("#preset" directive)."""
    if not presets:
        return body
    body = {} if body is None else body
    assert isinstance(body, dict), "a body built from presets must be a json object"
    refs = [f"#{{object:{name}}}" for name in presets]
    return {"#preset": refs[0] if len(refs) == 1 else refs, **body}


# ---------------------------------------------------------------------------
# Values
# ---------------------------------------------------------------------------

def _json_or_none(text):
    try:
        return json.loads(text)
    except (TypeError, ValueError):
        return None


def json_or_text(text):
    """A doc string or inline value: json when it parses, text otherwise."""
    if text is None:
        return None
    parsed = _json_or_none(text.strip())
    return text.strip() if parsed is None and text.strip() != "null" else parsed


def matches(actual, expected_text):
    """A value written in a step matches as json (0, true, {...}) or as text
    ("0" also matches the string "0"): the step reads as the value, whatever
    json type the agent keeps it with."""
    expected = _json_or_none(expected_text)
    if expected is not None or expected_text.strip() == "null":
        if actual == expected:
            return True
    return isinstance(actual, str) and actual == expected_text


def show(value):
    return json.dumps(value) if not isinstance(value, str) else value


def json_pointer(document, pointer):
    node = document
    for key in (k for k in pointer.split("/")[1:]):
        key = key.replace("~1", "/").replace("~0", "~")
        node = node[int(key)] if isinstance(node, list) else node[key]
    return node


def eventually(context, check):
    """Retry an assertion until it passes or the timeout expires: traffic is
    processed and client flows run asynchronously."""
    deadline = time.monotonic() + context.timeout
    while True:
        try:
            return check()
        except AssertionError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.1)


# ---------------------------------------------------------------------------
# Transformation tables: | from | filter | to |
# ---------------------------------------------------------------------------
#
# Each row is one grammar v2 rule. A cell is "<kind> [args]":
#   - "/..."      the json pointer ('path') of kinds taking one;
#   - "k=v"       any field by name;
#   - bare value  the kind's main field(s), in order (e.g. 'name' of a
#                 requestHeader, 'value' of a literal, 'rgx' then 'fmt' of a
#                 regexReplace); "not" sets the negation of a comparison.
# Quote ('...' or "...") values with spaces; single quotes keep backslashes.
# Several filters in one cell are separated by ';'.

MAIN_FIELDS = {
    # sources (from)
    "requestUriParam": ["name"], "requestHeader": ["name"], "math": ["expression"],
    "random": ["min", "max"], "randomset": ["values"], "timestamp": ["unit"],
    "strftime": ["fmt"], "var": ["name"], "vault": ["key"], "literal": ["value"],
    "txtFile": ["file"], "binFile": ["file"], "command": ["command"],
    # targets (to)
    "responseHeader": ["name"], "udpSocket": ["socket"], "clientProvision": ["id", "inState"],
    # filters
    "regexCapture": ["rgx"], "regexReplace": ["rgx", "fmt"], "regexKey": ["rgx"],
    "append": ["value"], "prepend": ["value"], "sum": ["value"], "multiply": ["value"],
    "conditionVar": ["name"], "equalTo": ["value"], "jsonConstraint": ["value"],
    "schemaId": ["id"], "baseConvert": ["in", "out"], "strptime": ["fmt"], "cast": ["type"],
}
PATH_KINDS = {"requestBody", "responseBody", "var", "vault"}
NEGATABLE = {"conditionVar", "equalTo"}
LIST_FIELDS = {"values"}  # every remaining bare value goes there

# Fields which are not text (everything else is, as the grammar wants it)
INTEGER_FIELDS = {"min", "max", "count", "size", "eventNumber", "recvseq", "sendseq",
                  "delayMs", "in", "out"}
BOOLEAN_FIELDS = {"not", "numeric", "capital"}
NUMBER_VALUE_KINDS = {"sum", "multiply"}
JSON_VALUE_KINDS = {"jsonConstraint"}


def _tokens(cell):
    """Split on blanks, honouring '...' (literal) and "..." (\\" escapes).
    Each token: (text, quoted, field) where field is the name of an unquoted
    'name=' prefix (then text is the value), or None."""
    tokens, current, quoted, quote, field, i = [], "", False, None, None, 0

    def close():
        tokens.append((current, quoted, field))

    while i < len(cell):
        c = cell[i]
        if quote:
            if c == quote:
                quote = None
            elif c == "\\" and quote == '"' and i + 1 < len(cell) and cell[i + 1] in '"\\':
                i += 1
                current += cell[i]
            else:
                current += c
        elif c in "'\"":
            quote, quoted = c, True
        elif c.isspace():
            if current or quoted:
                close()
            current, quoted, field = "", False, None
        elif c == "=" and field is None and not quoted and re.fullmatch(r"[A-Za-z]+", current):
            field, current = current, ""
        else:
            current += c
        i += 1
    assert quote is None, f"unbalanced quote in '{cell}'"
    if current or quoted or field:
        close()
    return tokens


def _split_outside_quotes(cell, sep):
    parts, current, quote = [], "", None
    for c in cell:
        if quote:
            quote = None if c == quote else quote
        elif c in "'\"":
            quote = c
        elif c == sep:
            parts.append(current)
            current = ""
            continue
        current += c
    parts.append(current)
    return [p.strip() for p in parts if p.strip()]


def _typed(kind, field, text, quoted):
    if field == "value" and kind in JSON_VALUE_KINDS:
        return json.loads(text)  # quoted or not: '{"a": 1}'
    if quoted:
        return text
    if field in BOOLEAN_FIELDS:
        assert text in ("true", "false"), f"{kind}: '{field}' must be true or false"
        return text == "true"
    if field in INTEGER_FIELDS and re.fullmatch(r"-?\d+", text):
        return int(text)
    if field == "value" and kind in NUMBER_VALUE_KINDS:
        number = _json_or_none(text)
        assert isinstance(number, (int, float)), f"{kind}: '{text}' is not a number"
        return number
    return text


def parse_cell(cell, discriminator):
    """'requestBody /name' -> {"source": "requestBody", "path": "/name"}"""
    tokens = _tokens(cell)
    assert tokens, "empty cell"
    kind, kind_quoted, kind_field = tokens[0]
    assert not kind_quoted and not kind_field, f"the kind must come first, unquoted: '{cell}'"
    item = {discriminator: kind}
    main = list(MAIN_FIELDS.get(kind, []))
    rest = []
    for text, quoted, field in tokens[1:]:
        if field:
            item[field] = _typed(kind, field, text, quoted)
        elif not quoted and text.startswith("/") and kind in PATH_KINDS:
            item["path"] = text
        elif not quoted and text == "not" and kind in NEGATABLE:
            item["not"] = True
        else:
            rest.append((text, quoted))
    for field in main:
        if field in item:
            continue
        if field in LIST_FIELDS:
            if rest:
                item[field] = [text for text, _ in rest]
                rest = []
        elif rest:
            text, quoted = rest.pop(0)
            item[field] = _typed(kind, field, text, quoted)
    assert not rest, (f"'{cell}': unexpected '{' '.join(t for t, _ in rest)}'"
                      f" ({kind} takes {', '.join(main) or 'no value'};"
                      " use name=value for other fields)")
    return item


def parse_rules(table):
    """A behave table with columns from, [filter,] to -> grammar v2 rules."""
    headings = [h.strip().lower() for h in table.headings]
    assert "from" in headings and "to" in headings, \
        "a transformation table needs the columns: from | filter | to (filter optional)"
    rules = []
    for row in table:
        cells = {h: row[i].strip() for i, h in enumerate(headings)}
        rule = {"from": parse_cell(cells["from"], "source")}
        if cells.get("filter"):
            rule["filter"] = [parse_cell(f, "op") for f in _split_outside_quotes(cells["filter"], ";")]
        rule["to"] = parse_cell(cells["to"], "target")
        rules.append(rule)
    return rules
