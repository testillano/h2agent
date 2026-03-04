The `client-endpoint.json` points to the local traffic server:

```json
{"id": "myServer", "host": "localhost", "port": 8000}
```

The `client-provision.json` starts with a static body and adds a transform to overwrite `bounce` with `sequence`:

```json
{
  "id": "ping",
  "endpoint": "myServer",
  "requestMethod": "POST",
  "requestUri": "/game/ping",
  "requestHeaders": {"content-type": "application/json"},
  "requestBody": {"bounce": 1},
  "transform": [
    {
      "source": "value.@{sequence}",
      "target": "request.body.json.integer./bounce"
    }
  ]
}
```

The `requestBody` provides the initial structure; the transform overwrites `/bounce` with the current sequence number on each execution. Without the transform, all 5 requests would have `{"bounce": 1}` and the test would fail on the last check.
