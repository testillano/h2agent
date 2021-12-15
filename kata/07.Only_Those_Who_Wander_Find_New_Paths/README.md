# Only Those Who Wander Find New Paths

During the response construction, it is normally needed to access request body or even the response body template, to manipulate information, modify and transfer it to different places. Many times, the data must be addressed by mean `json` paths as well as the target location path where the data must be stored.

So, you have these sources of information when it comes to use `json` paths:

- request.body: request body document from *root*.
- request.body.`<node1>..<nodeN>`: request body node path. This source path **admits variables substitution**.
- response.body: response body document from *root*. The use of provisioned response as template reference is rare but could ease the build of `json` structures for further transformations.
- response.body.`<node1>..<nodeN>`: response body node path. This source path **admits variables substitution**. The use of provisioned response as template reference is rare but could ease the build of `json` structures for further transformations.

And the targets:

- response.body.string *[string]*: response body document storing expected string at *root*.

- response.body.integer *[integer]*: response body document storing expected integer at *root*.

- response.body.unsigned *[unsigned integer]*: response body document storing expected unsigned integer at *root*.

- response.body.float *[float number]*: response body document storing expected float number at *root*.

- response.body.boolean *[boolean]*: response body document storing expected boolean at *root*.

- response.body.object *[json object]*: response body document storing expected object as *root* node.

- response.body.jsonstring *[json string]*: response body document storing expected object, extracted from json-parsed string, as *root* node.

- response.body.string.`<node1>..<nodeN>` *[string]*: response body node path storing expected string. This target path **admits variables substitution**.

- response.body.integer.`<node1>..<nodeN>` *[integer]*: response body node path storing expected integer. This target path **admits variables substitution**.

- response.body.unsigned.`<node1>..<nodeN>` *[unsigned integer]*: response body node path storing expected unsigned integer. This target path **admits variables substitution**.

- response.body.float.`<node1>..<nodeN>` *[float number]*: response body node path storing expected float number. This target path **admits variables substitution**.

- response.body.boolean.`<node1>..<nodeN>` *[boolean]*: response body node path storing expected booblean. This target path **admits variables substitution**.

- response.body.object.`<node1>..<nodeN>` *[json object]*: response body node path storing expected object under provided path. If source origin is not an object, there will be a best effort to convert to string, number, unsigned number, float number and boolean, in this specific priority order. This target path **admits variables substitution**.

- response.body.jsonstring.`<node1>..<nodeN>` *[json string]*: response body node path storing expected object, extracted from json-parsed string, under provided path. This target path **admits variables substitution**.

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

You must **complete** the `provision.json` file, in order to mirror the request body *as is* towards the response body, together with an additional `json` node called `processed` holding two child nodes within: `numbers` will be an array of the received integer values, and `appVersion` will store the value of the request header commented above.

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

