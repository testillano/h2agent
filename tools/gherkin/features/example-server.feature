Feature: h2agent as a mock server
  The scenarios provision the h2agent mock server and send it traffic
  directly ("When I send ..."), as the system under test would do.

  Background:
    Given server matching is "FullMatching"

  Scenario: Static answer
    Given the server answers "GET" "/api/v1/status" with 200
      """
      {"status": "ok", "version": "1.0"}
      """
    When I send "GET" to "/api/v1/status"
    Then the response code should be 200
      And the response header "content-type" should be "application/json"
      And the response body at "/status" should be "ok"

  Scenario: Text answer
    Given the server answers "GET" "/api/v1/ping" with 200 and body "pong"
    When I send "GET" to "/api/v1/ping"
    Then the response body should be "pong"

  Scenario: Echo request fields into the answer
    Given the server answers "POST" "/api/v1/users" with 201
      """
      {"id": 1}
      """
      And with transformations
        | from                      | filter        | to                       |
        | requestBody /name         |               | responseBody /name       |
        | timestamp ms              | cast unsigned | responseBody /createdMs  |
        | requestHeader x-trace-id  |               | responseHeader x-trace-id |
    When I send "POST" to "/api/v1/users" with body
      """
      {"name": "Alice"}
      """
    Then the response code should be 201
      And the response body at "/name" should be "Alice"
      And the response body at "/id" should be "1"

  Scenario: Values from the URI
    Given server matching is "RegexMatching"
      And the server answers "GET" "/api/v1/users/[0-9]+" with 200
        """
        {}
        """
      And with transformations
        | from                 | filter                                   | to                 |
        | requestUriPath       | regexReplace '/api/v1/users/([0-9]+)' $1 | var id             |
        | var id               | cast integer                             | responseBody /id   |
        | literal 'user-@{id}' |                                          | responseBody /name |
    When I send "GET" to "/api/v1/users/42"
    Then the response body at "/id" should be "42"
      And the response body at "/name" should be "user-42"

  Scenario: Answers which depend on the state
    Given the server answers "POST" "/api/v1/toggle" with 200 and body "on"
      And in state "initial" moving to "on"
      And the server answers "POST" "/api/v1/toggle" with 200 and body "off"
      And in state "on" moving to "initial"
    When I send "POST" to "/api/v1/toggle"
    Then the response body should be "on"
    When I send "POST" to "/api/v1/toggle"
    Then the response body should be "off"

  Scenario: Delayed answer and request validation
    Given the schema "user"
      """
      {"type": "object", "required": ["name"], "properties": {"name": {"type": "string"}}}
      """
      And the server answers "POST" "/api/v1/validated" with 200
      And with request schema "user"
      And with response delay 20 ms
    When I send "POST" to "/api/v1/validated" with body
      """
      {"name": "Bob"}
      """
    Then the response code should be 200
    When I send "POST" to "/api/v1/validated" with body
      """
      {"age": 3}
      """
    Then the response code should be 400

  Scenario: Inspect what the server received
    Given the server answers "POST" "/api/v1/items" with 201
    When I send "POST" to "/api/v1/items" with body
      """
      {"item": "first"}
      """
      And I send "POST" to "/api/v1/items" with body
      """
      {"item": "second"}
      """
    Then the server should have received 2 "POST" "/api/v1/items"
      And server data for "POST" "/api/v1/items" event -1 at "/requestBody/item" should be "second"
      And there should be 0 unused server provisions

  Scenario: Shared values (vault) and presets
    Given the vault entry "patterns"
      """
      {"id": "([0-9]+)"}
      """
      And the preset "pizza"
      """
      {"size": "medium", "toppings": {"cheese": "mozzarella", "sauce": "tomato"}}
      """
      And server matching is "RegexMatching"
      And the server answers "GET" "/api/v1/orders/#{patterns/id}" with 200 from preset "pizza"
      """
      {"toppings": {"cheese": "gorgonzola"}}
      """
    When I send "GET" to "/api/v1/orders/7"
    Then the response body should be
      """
      {"size": "medium", "toppings": {"cheese": "gorgonzola", "sauce": "tomato"}}
      """

  Scenario: Full json provision (escape hatch)
    Given server provision
      """
      {
        "requestMethod": "GET",
        "requestUri": "/api/v1/complex",
        "responseCode": 200,
        "responseBody": {"data": "complex"},
        "transform": [
          {"from": {"source": "recvseq"}, "filter": [{"op": "cast", "type": "unsigned"}],
           "to": {"target": "responseBody", "path": "/seq"}}
        ]
      }
      """
    When I send "GET" to "/api/v1/complex"
    Then the response body at "/data" should be "complex"
