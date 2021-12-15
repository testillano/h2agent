# Dynamic states

One interesting target type is the `outState`. It can be used to dynamically set the next state for the current event being processed. As in the case of the response delay, this transformation target overrides a possible provision `outState` definition.

With this resource we have full control to evolve the next provision to be executed.

## Exercise

Create the `provision.json` file, to manage a `POST` request to `URI` `/evolve` and two possible requests bodies: `{"direction": "up"}` or `{"direction": "down"}`.

That request must be answered with an empty body and a response header `item-number` with a number between 1 and 5 in such a way that `up` direction increases the count monotonically, and `down` direction decreases it. The initial state will have the count 1.
