2-step client flow using scoped variables instead of globalVar.

Demonstrates var.* propagation across outState links in client mode.
Link 1 captures a token from the server response into var.token.
Link 2 uses var.token in the request body — no globalVar, no cleanup.

Flow:
  1. Trigger fires provision (initial, authFlow) -> GET /api/v1/auth
  2. Server responds with {"token":"abc-123"}
     - onResponseTransform: captures response.body./token -> var.token
  3. State progression: initial -> authenticated
  4. Provision (authenticated, authFlow) fires -> POST /api/v1/data
     - Pre-send transform: injects var.token into request body
  5. Server responds with {"status":"ok"}

After execution, verify:
  - Client-data: 2 events (GET /auth, POST /data)
  - Server-data: POST /data received {"auth":"abc-123"} as body

Compare with ClientFlowTransform which uses globalVar for the same pattern.
Scoped var is simpler: no eraser needed, no namespace pollution.
