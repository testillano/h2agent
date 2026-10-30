# Foreign States: the database simulation

As you already know, as part of a provision is possible to apply a transformation to set the next execution state. This overrides the regular provision `outState`:

```json
{
  "requestMethod": "DELETE",
  "requestUri": "/the/uri/path",
  "responseCode": 200,
  "transform": [{"from": {"source": "literal", "value": "the-next-state"}, "to": {"target": "outState"}}],
  "outState": "this-will-be-overridden-by-target-outState-transformation"
}
```

But also, you could optionally specify a foreign method in the transformation target, for example `GET`:

```json
{
  "requestMethod": "DELETE",
  "requestUri": "/the/uri/path",
  "responseCode": 200,
  "transform": [
    {
      "from": {"source": "literal", "value": "the-next-state-for-GET"},
      "to": {"target": "outState", "method": "GET"}
    }
  ],
  "outState": "the-next-state-for-DELETEs"
}
```

This means, that a virtual event will be created for a `GET` request and will be initialized with the state `the-next-state-for-GET`.
So this acts like a `GET` is received and its state evolves following a foreign provision configuration. With this mechanism, it is possible to simulate things like a database where an element was removed.

The `outState` target accepts an optional `method` (`POST`, `GET`, `PUT`, `DELETE` or `HEAD`) and, with it, an optional `uri` (defaults to the current one): virtual server data will be created if needed, so that we can modify the flow for other methods (and URIs) different than the one which is managing the current provision. The `uri` **admits variables substitution**. Check the target in the [documentation](https://github.com/testillano/h2agent/blob/master/docs/api/README.md#transformation-pipeline).

## Exercise

Copy `server-matching.json` and `server-provision.json` files from  exercise "**08.Server_Data_History**".

Add the necessary provisions to `server-provision.json` to simulate the deletion of a random item.

The evaluation process will be:

* `POST` data for `/ctrl/v2/items/update/id-<number>`.

* `GET` data for that recently posted register with `URI` `/ctrl/v2/items/id-<number>`.

* `DELETE` data with `/ctrl/v2/items/id-<number>`, obtaining a `200` status code.

* `GET` data again expecting a `404` status code and no response data carried.


Think about possible improvements to the approach of this exercise .
