2-step client flow with transformations and data propagation.

Demonstrates pre-send transforms, on-response transforms, global variable
propagation between chained provisions, and state progression.

Flow:
  1. Trigger fires provision (initial, myFlow) -> GET /api/v1/hello
     - Pre-send transform: injects header x-request-id = "req-001"
  2. Server responds with {"greeting":"hello world"}
     - On-response transform: captures response.body -> globalVar.helloResponse
  3. State progression: initial -> step2
  4. Provision (step2, myFlow) fires -> POST /api/v1/goodbye
     - Pre-send transform: injects globalVar.helloResponse as request body
  5. Server responds with {"farewell":"see you"}
  6. outState="road-closed" -> chain stops

After execution, verify:
  - Client-data: 2 events, both with responseStatusCode=200
  - Server-data: POST /goodbye received {"greeting":"hello world"} as body
  - Global variables: helloResponse set to the hello response body
