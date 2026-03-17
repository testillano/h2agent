When we request a GET to the server with uri '/api/v1/resource',
we will obtain a status code 200 if the request contains the
mandatory headers (403 with failure detail if not):

  x-api-key: my-secret-key
  x-client-id: service-a

This example demonstrates the use of 'request.headers' source
combined with 'JsonConstraint' array filter to validate that
specific headers are present in the request.

The 'request.headers' source produces a JSON array of
{"name": "<key>", "value": "<val>"} objects. The JsonConstraint
array filter checks that every element in the expected array
exists somewhere in the received array, using partial object
matching.

Traffic classification: FullMatching (static URI).

The interactive request.sh script will prompt for header values
on each iteration. Leave empty to omit a header and observe the
403 response with the constraint failure report.
