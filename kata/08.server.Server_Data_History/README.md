# Server Data History

Server internal data (requests received and their states and other useful information like timing) are exposed through the agent `REST API`, but are also accessible at transformation filters using the source type `serverEvent`. Let's see an example.

Imagine the following current server data map:

```json
[
  {
    "method": "POST",
    "events": [
      {
        "requestBody": {
          "engine": "tdi",
          "model": "audi",
          "year": 2021
        },
        "requestHeaders": {
          "accept": "*/*",
          "content-length": "52",
          "content-type": "application/x-www-form-urlencoded",
          "user-agent": "curl/7.77.0"
        },
        "previousState": "initial",
        "receptionTimestampUs": 1626039610709567,
        "responseDelayMs": 0,
        "responseStatusCode": 201,
        "recvseq": 116,
        "state": "initial"
      }
    ],
    "uri": "/app/v1/stock/madrid?loc=123"
  }
]
```

**Note**: remember that `./tools/helpers.bash` can be sourced to access some helper functions, for example you could execute `server_data && json`, which dumps the current server data snapshot in pretty `json` format like the previous example.

Now, you can prepare an event source to access the request body for the last event in events history shown above (the last is the first as only one event is registered). Each address field is a plain json value, so the request *URI* is written as it is, query parameters included:

	`{"source": "serverEvent", "requestMethod": "POST", "requestUri": "/app/v1/stock/madrid?loc=123", "eventNumber": -1, "eventPath": "/requestBody"}`

Then, the event source would store this `json` object:

```json
{
  "engine": "tdi",
  "model": "audi",
  "year": 2021
}
```

Now, just configure a provision to extract such object and transfer it to wherever you want, for example, the response body of a `GET` to the same *URI*:

```json
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/stock/madrid?loc=123",
  "responseCode": 200,
  "transform": [
    {
      "from": {"source": "requestUri"},
      "to": {"target": "var", "name": "eventRequestUri"}
    },
    {
      "from": {
        "source": "serverEvent",
        "requestMethod": "POST",
        "requestUri": "@{eventRequestUri}",
        "eventNumber": -1,
        "eventPath": "/requestBody"
      },
      "filter": [{"op": "cast", "type": "object"}],
      "to": {"target": "responseBody"}
    }
  ]
}
```

Note that, instead of writing the request *URI* in the event source, we take it from the request received (`{"source": "requestUri"}`, the *URI* with its query parameters) into a variable (`eventRequestUri`): every address field admits variables substitution, so the address can be built from previous transformations. The same provision then serves any stock location without changes in the event address (with a matching algorithm accepting those *URIs*).

## Exercise

Create the `server-provision.json` to accept these requests:

* `POST` `/ctrl/v2/items/update/id-<number>`
* `GET` `/ctrl/v2/items/id-<number>`

The `POST` request must be answered with status code `200`.

The `GET` request must be answered with status code `200` and a response body which is the request body received in the previous `POST` request.

The `server-matching.json` configuration is provided as a `RegexMatching` algorithm, although the order is not significant here because the `URIs` received are different. As those `URIs` transport an arbitrary identifier, this discards the `FullMatching` algorithm, and we must match them to identify the corresponding provision. This algorithm is normally used when a server manages different `URIs` which cannot be processed with a master/unique regular expression (`FullMatchingRegexReplace` algorithm).

### Hints

Open them one by one, only when you get stuck:

<details>
<summary>Hint 1</summary>

You need two provisions (`POST` and `GET`); as `RegexMatching` is used, their `requestUri` are regular expressions.

</details>

<details>
<summary>Hint 2</summary>

The `GET` must read the body of a past `POST`: look at the `serverEvent` source, which addresses stored events by method, URI and event number (`-1` is the last one), plus a json pointer inside the event (`eventPath`).

</details>

<details>
<summary>Hint 3</summary>

The `POST` URI contains the same id as the `GET` one: capture it from the `GET` URI with a `regexCapture` filter into a variable (groups are `<name>.1`, `<name>.2`...) and use `@{...}` to build the `POST` URI.

</details>

<details>
<summary>Hint 4</summary>

The event body is an object: write it to the response body root with the `object` cast.

</details>
