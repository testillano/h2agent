# docs/api — Maintenance

How to maintain the REST API documentation for h2agent.

## Files

| File | Description |
|---|---|
| `openapi.yaml` | OpenAPI 3.1 specification (28 paths, 45 operations, 19 schemas) |
| `README.md` | User guide — conceptual docs for matching, FSM, transformations, triggering, data querying |
| `index.html` | Redoc renderer (zero-build, CDN-based). Served via GitHub Pages at `/api/` |
| `serve-api-docs.sh` | Local preview with `docker run nginx:alpine` |
| `../../tools/check_schema_drift.py` | Schema drift detector (see below). Lives under `tools/`, not here. |

## Schemas in openapi.yaml

### Auto-generatable (from running server)

These schemas match the JSON Schemas returned by h2agent's `/admin/v2/api-schema/<resource>` endpoints.
They can be checked for drift with `check_schema_drift.py` (it prints the server schemas; it
does not rewrite `openapi.yaml`).

| Schema | Source endpoint |
|---|---|
| `SchemaConfig` | `/admin/v2/api-schema/schema` |
| `VaultMap` | `/admin/v2/api-schema/vault` |
| `ServerMatching` | `/admin/v2/api-schema/server-matching` |
| `ServerProvision` | `/admin/v2/api-schema/server-provision` |
| `ClientEndpoint` | `/admin/v2/api-schema/client-endpoint` |
| `ClientProvision` | `/admin/v2/api-schema/client-provision` |

The server also provides the transformation grammar (v2) as definitions inside the provision
schemas (`filter`, `from`, `to`, `rule`, and `responseFrom`/`responseTo`/`responseRule` for client
provisions). In the OpenAPI spec these are extracted as separate components for readability:
`TransformFilter`, `ServerTransformSource`/`Target`/`Item`, `ClientRequestTransformSource`/`Target`/`Item`
and `ClientResponseTransformSource`/`Target`/`Item` (draft-07 `dependencies` becomes `dependentRequired`).

### Manual (inferred from responses)

These schemas are not exposed by the server via `/schema` endpoints.
They were inferred from actual GET responses and must be maintained by hand.

| Schema | Used by |
|---|---|
| `ResultResponse` | All POST/PUT/DELETE responses (`{result, response}`) |
| `LogLevel` | `GET /admin/v2/logging` (plain text enum) |
| `GeneralConfiguration` | `GET /admin/v2/configuration` |
| `ServerConfiguration` | `GET /admin/v2/server/configuration` |
| `DataStorageConfiguration` | `GET /admin/v2/server-data/configuration`, `GET /admin/v2/client-data/configuration` |
| `ServerDataSummary` | `GET /admin/v2/server-data/summary` |
| `ClientDataSummary` | `GET /admin/v2/client-data/summary` |
| `FileInfo` | `GET /admin/v2/files` |

## Schema drift detection

When C++ schemas change, regenerate and compare:

```bash
# Start h2agent, then:
python3 tools/check_schema_drift.py http://localhost:8074 > /tmp/generated.yaml

# Compare (descriptions and extracted components will differ — focus on types, patterns, enums)
diff <(python3 -c "
import yaml
with open('docs/api/openapi.yaml') as f:
    print(yaml.dump(yaml.safe_load(f)['components']['schemas'], sort_keys=True))
") <(python3 -c "
import yaml
with open('/tmp/generated.yaml') as f:
    print(yaml.dump(yaml.safe_load(f)['components']['schemas'], sort_keys=True))
")
```

The script uses `curl --http2-prior-knowledge`.

## Local preview

```bash
./serve-api-docs.sh    # opens http://localhost:8001
```
