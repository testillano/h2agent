# Foreign States: the database simulation

As you already know, as part of a provision is possible to apply a transformation to set the next execution state. This overrides the regular provision `outState`:

```json
{
  "requestMethod": "DELETE",
  "requestUri": "/the/uri/path",
  "responseCode": 200,
  "transform": [
    {
      "source": "value.the-next-state",
      "target": "outState"
    }
  ],
  "outState": "this-will-be-overridden-by-target-outState-transformation"
}
```

But also, you could optionally specify a foreign method in the transformation target, for example `outState.GET`:

```json
{
  "requestMethod": "DELETE",
  "requestUri": "/the/uri/path",
  "responseCode": 200,
  "transform": [
    {
      "source": "value.the-next-state-for-GET",
      "target": "outState.GET"
    }
  ],
  "outState": "the-next-state-for-DELETEs"
}
```

This means, that a virtual event will be created for a `GET` request and will be initialized with the state `the-next-state-for-GET`.
So this acts like a `GET` is received and its state evolves following a foreign provision configuration. With this mechanism, it is possible to simulate things like a database where an element was removed.

This is the complete target specification for the foreign state definition:

outState.`[POST|GET|PUT|DELETE|HEAD][.<uri>]` *[string (or number as string)]*: next processing state for specific method (virtual server data will be created if needed: this way we could modify the flow for other methods different than the one which is managing the current provision). This target **admits variables substitution** in the `uri` part.

## Exercise

Copy `server-matching.json` and `server-provision.json` files from  exercise "**08.Server_Data_History**".

Add the necessary provisions to `server-provision.json` to simulate the deletion of a random item.

The evaluation process will be:

* `POST` data for `/ctrl/v2/items/update/id-<number>`.

* `GET` data for that recently posted register with `URI` `/ctrl/v2/items/id-<number>`.

* `DELETE` data with `/ctrl/v2/items/id-<number>`, obtaining a `200` status code.

* `GET` data again expecting a `404` status code and no response data carried.


Think about possible improvements to the approach of this exercise .
