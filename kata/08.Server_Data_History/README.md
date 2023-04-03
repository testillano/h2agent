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
        "serverSequence": 116,
        "state": "initial"
      }
    ],
    "uri": "/app/v1/stock/madrid?loc=123"
  }
]
```

**Note**: remember that `./tools/helpers.src` can be sourced to access some helper functions, for example you could execute `server-data && json`, which dumps the current server data snapshot in pretty `json` format like the previous example.

Now, you can prepare an event source to access the request body for the last event in events history shown above (the last is the first as only one event is registered). Take into account the request *URI* must be encoded in this case (you can use the utility `./tools/url.sh` within the project):

​	`serverEvent.requestMethod=POST&requestUri=/app/v1/stock/madrid%3Floc%3D123&eventNumber=-1&eventPath=/requestBody`

Then, the event source would store this `json` object:

```json
{
  "engine": "tdi",
  "model": "audi",
  "year": 2021
}
```

Now, just configure a provision to extract such object and transfer it to wherever you want, for example, the response body:

```json
{
  "requestMethod": "GET",
  "requestUri": "/app/v1/stock/madrid?loc=123",
  "responseCode": 200,
  "transform": [
    {
      "source": "value./app/v1/stock/madrid?loc=123",
      "target": "var.eventRequestUri"
    },
    {
      "source": "serverEvent.requestMethod=POST&requestUri=@{eventRequestUri}&eventNumber=-1&eventPath=/requestBody",
      "target": "response.body.json.object"
    }
  ]
}
```

Note that, instead of defining a server event source with the request *URI* encoded, we decided to define a variable for the *URI*  (`eventRequestUri`) which will be replaced after query parameters interpretation (skipping the potential ambiguity problem). Anyway, you may use the single transformation with the encoded information for the *URI*.

## Exercise

Create the `server-provision.json` to accept these requests:

* `POST` `/ctrl/v2/items/update/id-<number>`
* `GET` `/ctrl/v2/items/id-<number>`

The `POST` request must be answered with status code `200`.

The `GET` request must be answered with status code `200` and a response body which is the request body received in the previous `POST` request.

The `server-matching.json` configuration is provided as a `RegexMatching` algorithm, although the order is not significant here because the `URIs` received are different. As those `URIs` transport an arbitrary identifier, this discards the `FullMatching` algorithm, and we must match them to identify the corresponding provision. This algorithm is normally used when a server manages different `URIs` which cannot be processed with a master/unique regular expression (`FullMatchingRegexReplace` algorithm).
