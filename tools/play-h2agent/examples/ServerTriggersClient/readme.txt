Server-triggered client flow example.

An external client POSTs to /api/v1/webhook/notify.
The server provision responds 200 and triggers client provision "forwardNotification".
The client provision POSTs the notification body to /api/v1/forward (loopback).
A second server provision handles the forwarded request and responds 200.

Flow:
  external POST /api/v1/webhook/notify
    → server responds 200 {"status":"received"}
    → triggers clientProvision.forwardNotification
      → client POSTs /api/v1/forward with the original notification body
        → server responds 200 {"status":"forwarded"}
