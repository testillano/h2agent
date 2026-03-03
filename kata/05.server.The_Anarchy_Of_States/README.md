# The Anarchy Of States: who need them ?

The `h2agent` provides an internal `FSM` (finite state machine) for the provisions model. This means that we could evolve the state for an specific `URI` reception in order to change the reaction for that same event in different moments. In this way we cover many scenarios in which several indistinguishable requests reacts with different responses.

Provisions have two important fields in the `json` document: `inState` and `outState`. A maiden event received (which is not found in the server data history) will search for a provision with the `inState` storing the reserved value `initial`, so <u>it is important to have a provision item with this value</u> or it will never be found (you could omit the `inState` field in your provision as it defaults to `initial`). After processing the provision, the event will update its internal state to the value given by the `outState` field for that provision (again, it defaults to `initial` if missing). In this manner, next incoming event for that same `URI` will get it as current state, and will search a provision with an `inState` having such value. The procedure is repeated while evolving the states in further provisions as desired.

Here you have a funny conversation example:

> Client > hello
>
> Server > hello, how are you ?
>
> Client > hello
>
> Server > hello, hello !
>
> Client > hello
>
> Server > are you OK ?

```json
[
  {
    "outState": "second",
    "requestMethod": "GET",
    "requestUri": "/hello",
    "responseCode": 200,
    "responseBody": "hello, how are you ?"
  },
  {
    "inState": "second",
    "outState": "third",
    "requestMethod": "GET",
    "requestUri": "/hello",
    "responseCode": 200,
    "responseBody": "hello, hello !"
  },
  {
    "inState": "third",
    "outState": "four",
    "requestMethod": "GET",
    "requestUri": "/hello",
    "responseCode": 200,
    "responseBody": "are you OK ?"
  }
]
```

The first provision item omits the `inState`, but you could add `"inState":"initial"` to be more clear if you want.

Another important thing about this provision set is that when exhausted, further requests from the client will never be answered (just a `501` which means *Not Implemented*, which is the way `h2agent` indicates a missing provision for the received event).

So, that state called `four` is a kind of *road closed* for the dialogue because there is not a defined provision with `four` as `inState` field value.

There is another interesting reserved state called `purge`. It will remove all the history related to the event `URI` processed, so it can be reused (next request would be a maiden event and this has a correct provision: the first one: `initial`).

Normally, in load testing, we should never repeat a cycle reusing past `URIs`. The final `purge` state should remove the completed transactions, leaving available only the failed ones. Then, at the end of the test, all the survival server data events shall be used for forensics analysis as you could see the states where the flow was stuck to blame the proper component within the system under test.

## Exercise

Fix the `server-provision.json` file, in order to reassemble a coherent quote built from three `GET` requests on `rothbard/says` `URI`.
