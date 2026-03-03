# Dynamic Request Body

When sending multiple requests, it is often useful to include a sequence number in the body — for tracing, ordering, or simply to verify that each request was sent correctly.

In client mode, `h2agent` exposes the current execution counter via `sequence`. This value increments on each send and can be used in transforms to build dynamic request bodies.

## sequence

When a provision is triggered with a range:

```
GET /admin/v1/client-provision/sender?sequenceBegin=1&sequenceEnd=5&rps=2
```

The provision fires 5 times. On each execution, `sequence` holds the current counter value (1, 2, 3, 4, 5).

You can use it in a transform to set a field in the request body:

```json
{
  "source": "value.@{sequence}",
  "target": "request.body.json.integer./counter"
}
```

## The server

The server provision is already configured (`server-provision.json`). It echoes back the request body in the response.

## Exercise

Complete `client-provision.json` to send `POST /game/ping` with body `{"bounce": <sequence_number>}`.

The `test.sh` triggers 5 requests and verifies that the last one had `bounce=5` in the request body received by the server.

**Hint**: add a `transform` entry to set the `bounce` field with `sequence`. The transform creates the JSON node automatically — no need for a static `requestBody`.
