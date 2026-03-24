#!/usr/bin/env python3
"""Fetch JSON Schemas from a running h2agent and generate openapi.yaml schemas section.

Usage: python3 generate_openapi_schemas.py [base_url]
Default base_url: http://localhost:8074
"""
import json, subprocess, sys, yaml

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8074"

SCHEMA_ENDPOINTS = {
    "/admin/v1/schema/schema":           "SchemaConfig",
    "/admin/v1/vault/schema":  "VaultMap",
    "/admin/v1/server-matching/schema":  "ServerMatching",
    "/admin/v1/server-provision/schema": "ServerProvision",
    "/admin/v1/client-endpoint/schema":  "ClientEndpoint",
    "/admin/v1/client-provision/schema": "ClientProvision",
}

def fetch(path):
    url = BASE + path
    try:
        r = subprocess.run(
            ["curl", "-s", "--http2-prior-knowledge", url],
            capture_output=True, text=True, timeout=5)
        return json.loads(r.stdout) if r.stdout else None
    except Exception as e:
        print(f"WARN: {url} → {e}", file=sys.stderr)
        return None

def clean_schema(s):
    """Convert JSON Schema to OpenAPI 3.1 component: extract definitions, rewrite $ref."""
    if not isinstance(s, dict):
        return s, {}
    extracted = {}

    # Pull inline definitions → separate components
    for name, defn in s.pop("definitions", {}).items():
        camel = name[0].upper() + name[1:]
        clean, sub = clean_schema(defn)
        extracted[camel] = clean
        extracted.update(sub)

    # Drop $id and $schema (JSON Schema meta, not needed in OpenAPI)
    s.pop("$id", None)
    s.pop("$schema", None)

    # Rewrite $ref paths
    if "$ref" in s:
        ref = s["$ref"]
        if ref.startswith("#/definitions/"):
            leaf = ref.split("/")[-1]
            s["$ref"] = f"#/components/schemas/{leaf[0].upper() + leaf[1:]}"

    # Recurse
    for key in ("properties", "patternProperties"):
        if key in s:
            for pname in s[key]:
                s[key][pname], sub = clean_schema(s[key][pname])
                extracted.update(sub)
    for key in ("items", "additionalProperties"):
        if key in s and isinstance(s[key], dict):
            s[key], sub = clean_schema(s[key])
            extracted.update(sub)
    for key in ("oneOf", "anyOf", "allOf"):
        if key in s:
            for i, item in enumerate(s[key]):
                s[key][i], sub = clean_schema(item)
                extracted.update(sub)

    return s, extracted

def main():
    all_schemas = {}
    for path, name in SCHEMA_ENDPOINTS.items():
        raw = fetch(path)
        if raw is None:
            continue
        cleaned, extracted = clean_schema(raw)
        all_schemas[name] = cleaned
        all_schemas.update(extracted)

    print(yaml.dump({"components": {"schemas": all_schemas}},
                    default_flow_style=False, allow_unicode=True,
                    sort_keys=False, width=200))

if __name__ == "__main__":
    main()
