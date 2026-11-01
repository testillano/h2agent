# Dynamic Request Body

When sending multiple requests, it is often useful to include a sequence number in the body — for tracing, ordering, or simply to verify that each request was sent correctly.

In client mode, `h2agent` exposes the current execution counter via `sequence`. This value increments on each send and can be used in transforms to build dynamic request bodies.

## sequence

When a provision is triggered with a range:

```
POST /admin/v2/client-provision/sender/trigger?sequenceBegin=1&sequenceEnd=5&cps=2
```

The provision fires 5 times. On each execution, `sequence` holds the current counter value (1, 2, 3, 4, 5).

You can use it in a transform to set a field in the request body:

```json
{
  "from": {"source": "literal", "value": "@{sequence}"},
  "to": {"target": "requestBody", "path": "/counter"}
}
```

This builds `{"counter": "<sequence>"}`: the node gets the value as the source yields it (a `literal` is a text).

## The server

The server provision is already configured (`server-provision.json`). It echoes back the request body in the response.

## Exercise

Complete `client-provision.json` to send `POST /game/ping` with body `{"bounce": <sequence_number>}`.

The `test.sh` triggers 5 requests and verifies that the last one had `bounce` equal to the **number** `5` (not the string `"5"`) in the request body received by the server.

**Hint**: a `transform` entry creates the json node automatically — no need for a static `requestBody`. Take care of the value type: a json body target keeps the type of the value it receives, and the transformation filters can convert it.
