# Knock Knock

Some doors only open if you knock first. This kata models a server that requires a specific sequence of requests: you must `POST /knock` before you can `GET /enter`. If you try to enter without knocking, you get a `403 Forbidden`.

## States as a protocol

The server uses `inState`/`outState` to enforce the knock-before-enter protocol:

- Initial state: the door is closed. A `GET /enter` returns `403`.
- After `POST /knock`: the door is open. A `GET /enter` returns `200`.
- After `GET /enter`: the door closes again (back to initial).

This is a classic use of the state machine to model a simple protocol.

## Exercise

The `server-matching.json` is provided. Create both:

- `client-provision.json` with two provisions using endpoint `myServer`:
  - `try-enter`: `GET /enter`
  - `knock`: `POST /knock`

  Use `expectedResponseStatusCode` on each provision to validate the server response inline (e.g. `200` for knock, `200` for enter after knocking).

- `server-provision.json` with the three provisions that implement the protocol above.

The `test.sh` verifies the full sequence.

**Hint**: the `POST /knock` needs to set a foreign state for `GET /enter` using `outState.GET./enter`, since they are different URIs/methods.
