# docs/api — Maintenance

How to maintain the REST API documentation for h2agent.

## Files

| File | Description |
|---|---|
| `openapi.yaml` | OpenAPI 3.1 specification (25 paths, 42 operations, 18 schemas) |
| `README.md` | User guide — conceptual docs for matching, FSM, transformations, triggering, data querying |
| `index.html` | Redoc renderer (zero-build, CDN-based). Served via GitHub Pages at `/api/` |
| `serve-api-docs.sh` | Local preview with `docker run nginx:alpine` |
| `generate_openapi_schemas.py` | Schema drift detector (see below) |

## Schemas in openapi.yaml

### Auto-generatable (from running server)

These schemas match the JSON Schemas returned by h2agent's `/admin/v1/*/schema` endpoints.
They can be regenerated with `generate_openapi_schemas.py` for drift detection.

| Schema | Source endpoint |
|---|---|
| `SchemaConfig` | `/admin/v1/schema/schema` |
| `GlobalVariableMap` | `/admin/v1/global-variable/schema` |
| `ServerMatching` | `/admin/v1/server-matching/schema` |
| `ServerProvision` | `/admin/v1/server-provision/schema` |
| `ClientEndpoint` | `/admin/v1/client-endpoint/schema` |
| `ClientProvision` | `/admin/v1/client-provision/schema` |

The server also provides `TransformFilter` (as `filter` definition) and transform item
structures inline within provision schemas. In the OpenAPI spec these are extracted as
separate components (`ServerTransformItem`, `ClientRequestTransformItem`,
`ClientResponseTransformItem`, `TransformFilter`) for readability.

### Manual (inferred from responses)

These schemas are not exposed by the server via `/schema` endpoints.
They were inferred from actual GET responses and must be maintained by hand.

| Schema | Used by |
|---|---|
| `ResultResponse` | All POST/PUT/DELETE responses (`{result, response}`) |
| `LogLevel` | `GET /admin/v1/logging` (plain text enum) |
| `GeneralConfiguration` | `GET /admin/v1/configuration` |
| `ServerConfiguration` | `GET /admin/v1/server/configuration` |
| `DataStorageConfiguration` | `GET /admin/v1/server-data/configuration`, `GET /admin/v1/client-data/configuration` |
| `ServerDataSummary` | `GET /admin/v1/server-data/summary` |
| `ClientDataSummary` | `GET /admin/v1/client-data/summary` |
| `FileInfo` | `GET /admin/v1/files` |

## Schema drift detection

When C++ schemas change, regenerate and compare:

```bash
# Start h2agent, then:
python3 docs/api/generate_openapi_schemas.py http://localhost:8074 > /tmp/generated.yaml

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
./serve-api-docs.sh    # opens http://localhost:8080
```
