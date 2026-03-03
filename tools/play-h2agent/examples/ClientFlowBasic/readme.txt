Basic 2-step client flow with loopback (h2agent as both client and server).

Demonstrates automatic state progression: when a client provision's outState
differs from its inState, the system finds the next provision matching
(outState, id) and triggers it automatically upon receiving the response.

Flow:
  1. Trigger fires provision (initial, myFlow) -> GET /api/v1/hello
  2. Server responds with {"greeting":"hello world"}
  3. outState="step2" != inState="initial" -> state progression
  4. Provision (step2, myFlow) fires -> POST /api/v1/goodbye
  5. Server responds with {"farewell":"see you"}
  6. outState="road-closed" -> chain stops

After execution, check client-data for 2 events and server-data for 2 received requests.
