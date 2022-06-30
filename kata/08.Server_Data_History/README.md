# Server Data History

Server internal data (requests received and their states and other useful information like timing) are exposed through the agent `REST API`, but are also accessible at transformation filters using the source type `event`. Let's see an example.

Imagine the following current server data map:

```json
[
  {
    "method": "POST",
    "requests": [
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

Now, you can prepare an event source `event.ev1`, just defining four variables to address the event whose names extend the event variable with *method*, *uri*, *number* and *path* suffixes:

​	`ev1.method` = "POST"

​	`ev1.uri` = "/app/v1/stock/madrid?loc=123"

​	`ev1.number` = -1 (means "the last")

​	`ev1.path` = "/requestBody"

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
      "source": "value.POST",
      "target": "var.ev1.method"
    },
    {
      "source": "value./app/v1/stock/madrid?loc=123",
      "target": "var.ev1.uri"
    },
    {
      "source": "value.-1",
      "target": "var.ev1.number"
    },
    {
      "source": "value./requestBody",
      "target": "var.ev1.path"
    },
    {
      "source": "event.ev1",
      "target": "response.body.object"
    }
  ]
}
```

## Exercise

Create the `server-provision.json` to accept these requests:

* `POST` `/ctrl/v2/items/update/id-<number>`
* `GET` `/ctrl/v2/items/id-<number>`

The `POST` request must be answered with status code `200`.

The `GET` request must be answered with status code `200` and a response body which is the request body received in the previous `POST` request.

The `server-matching.json` configuration is provided as a `RegexMatching` algorithm, although the order is not significant here because the `URIs` received are different. As those `URIs` transport an arbitrary identifier, this discards the `FullMatching` algorithm, and we must match them to identify the corresponding provision. This algorithm is normally used when a server manages different `URIs` which cannot be processed with a master/unique regular expression (`FullMatchingRegexReplace` algorithm).
