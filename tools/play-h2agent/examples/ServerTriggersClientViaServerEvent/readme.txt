Server-triggered client flow using serverEvent source (correlation pattern).

An external client POSTs to /api/v1/webhook/notify with a JSON body.
The server provision responds 200 and triggers client provision "forwardNotification".
The client provision reads the original request body directly from the server event
(using serverEvent source) and POSTs it to /api/v1/forward — no vault needed.
A second server provision handles the forwarded request and responds 200.

This is the recommended approach when the client provision needs to access
server request data without copying fields to intermediate variables.

Flow:
  external POST /api/v1/webhook/notify {"event":"login","userId":"u42"}
    → server responds 200 {"status":"received"}
    → triggers clientProvision.forwardNotification
      → client reads serverEvent.POST./api/v1/webhook/notify.0.body
      → client POSTs /api/v1/forward {"event":"login","userId":"u42"}
        → server responds 200 {"status":"forwarded"}

The client provision uses expectedResponseStatusCode=200 to validate the
forwarded response inline — if the server returns an unexpected status code,
the validation failure counter increments.
