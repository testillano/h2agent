# Only Those Who Wander Find New Paths

During the response construction, it is normally needed to access request body or even the response body template, to manipulate information, modify and transfer it to different places. Many times, the data must be addressed by mean `json` paths as well as the target location path where the data must be stored.

Body sources and targets address their nodes with an optional `path` ([json pointer](https://tools.ietf.org/html/rfc6901)); when it is missing, the whole document is used:

- `{"source": "requestBody"[, "path": "/<node1>/../<nodeN>"]}`: the request body received (or one of its nodes).
- `{"source": "responseBody"[, "path": "/<node1>/../<nodeN>"]}`: the response body being built (initially, the provisioned one, which could be used as a template).
- `{"target": "responseBody"[, "path": "/<node1>/../<nodeN>"]}`: the response body (or one of its nodes) where the value is stored.

The value keeps the type the source yields. A final `cast` filter converts it when needed: `{"op": "cast", "type": "string|integer|unsigned|float|boolean|object|json"}`.

Paths **admit variables substitution**. Check all the sources, filters and targets in the [documentation](https://github.com/testillano/h2agent/blob/master/docs/api/README.md#transformation-pipeline).

## Exercise

Consider a `POST` request to `URI` `/process/numbers` with this `json` body content (three random integers):

```json
{
    "number1": 13495,
    "number2": 19284,
    "number3": 2399
}
```

and also carrying a header `app-version` with the server business logic semantic version (string format).

You must **complete** the `server-provision.json` file, in order to mirror the request body *as is* towards the response body, together with an additional `json` node called `processed` holding two child nodes within: `numbers` will be an array of the received integer values, and `appVersion` will store the value of the request header commented above.

So, the response body should be something like:

```json
{
    "number1": 13495,
    "number2": 19284,
    "number3": 2399,
    "processed": {
        "appVersion": "1.0.1",
        "numbers": [ 13495, 19284, 2399 ]
    }
}
```

