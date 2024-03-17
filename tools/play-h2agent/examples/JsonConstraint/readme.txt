When we request a GET to the server with uri '/validate',
we will obtain a status code 200 if response is validated
against this body (417 if not):

{
  "id": "test_2",
  "randoms": [
    "2", "2"
  ]
}

Traffic classification (matching configuration) will be simple, FullMatching,
as the example is done for an static/constant and hence, predictable URI.

Traffic behavior (provision configuration) will configure a response which
depends on random number <R> between 1 and 5:

{
  "id": "test_<R>",
  "foo": "bar",
  "randoms": [
    "<R>", "2"
  ],
  "completed": true
}
