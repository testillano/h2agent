Feature: h2agent Gherkin driver examples

  Background:
    Given server matching is "FullMatching"

  # -----------------------------------------------------------------
  # 1. Static response
  # -----------------------------------------------------------------
  Scenario: Static response
    Given a server provision for "GET" on "/api/v1/status"
      And with response code 200
      And with response body
        """
        {"status": "ok", "version": "1.0"}
        """
    When the server provision is committed
      And I send "GET" to "/api/v1/status"
    Then the response code should be 200
      And the response body at "/status" should be "ok"

  # -----------------------------------------------------------------
  # 2. Echo transform
  # -----------------------------------------------------------------
  Scenario: Echo request field into response
    Given a server provision for "POST" on "/api/v1/users"
      And with response code 201
      And with response body
        """
        {"id": 1}
        """
      And with response header "content-type" as "application/json"
      And with transform from "request.body./name" to "response.body.json.string./name"
    When the server provision is committed
      And I send "POST" to "/api/v1/users" with body
        """
        {"name": "Alice"}
        """
    Then the response code should be 201
      And the response body at "/name" should be "Alice"

  # -----------------------------------------------------------------
  # 3. FSM toggle
  # -----------------------------------------------------------------
  Scenario: Toggle between states
    Given a server provision for "POST" on "/api/v1/toggle"
      And with inState "initial"
      And with outState "toggled"
      And with response code 200
      And with response body "off"
    When the server provision is committed
    Given a server provision for "POST" on "/api/v1/toggle"
      And with inState "toggled"
      And with outState "initial"
      And with response code 200
      And with response body "on"
    When the server provision is committed
      And I send "POST" to "/api/v1/toggle"
    Then the response body should be "off"
    When I send "POST" to "/api/v1/toggle"
    Then the response body should be "on"

  # -----------------------------------------------------------------
  # 4. JSON escape hatch
  # -----------------------------------------------------------------
  Scenario: Full JSON provision
    Given server provision
      """
      {
        "requestMethod": "GET",
        "requestUri": "/api/v1/complex",
        "responseCode": 200,
        "responseBody": {"data": "complex"},
        "transform": [
          {"source": "timestamp.ms", "target": "response.body.json.unsigned./ts"},
          {"source": "recvseq", "target": "response.body.json.unsigned./seq"}
        ]
      }
      """
    When I send "GET" to "/api/v1/complex"
    Then the response code should be 200
      And the response body at "/data" should be "complex"

  # -----------------------------------------------------------------
  # 5. Query parameters matching
  # -----------------------------------------------------------------
  Scenario: Sort query parameters for predictable matching
    Given server matching is "FullMatching" with query parameters filter "Sort"
      And a server provision for "GET" on "/api/v1/search?color=red&size=large"
      And with response code 200
      And with response body "found"
    When the server provision is committed
      And I send "GET" to "/api/v1/search?size=large&color=red"
    Then the response code should be 200
      And the response body should be "found"

  Scenario: Ignore query parameters
    Given server matching is "FullMatching" with query parameters filter "Ignore"
      And a server provision for "GET" on "/api/v1/items"
      And with response code 200
    When the server provision is committed
      And I send "GET" to "/api/v1/items?page=1&limit=10"
    Then the response code should be 200

  # -----------------------------------------------------------------
  # 6. Vault
  # -----------------------------------------------------------------
  Scenario: Vault operations
    Given vault "counter" with value "0"
    Then vault "counter" should be "0"
    Given vault "counter" with value "42"
    Then vault "counter" should be "42"
    When vault "counter" is cleared
      And I GET admin "vault"
    Then the response code should be 204

  # -----------------------------------------------------------------
  # 7. Schema validation
  # -----------------------------------------------------------------
  Scenario: Validate request with schema
    Given a schema "user-schema"
      """
      {
        "type": "object",
        "required": ["name"],
        "properties": {
          "name": {"type": "string"}
        }
      }
      """
      And a server provision for "POST" on "/api/v1/validated"
      And with request schema "user-schema"
      And with response code 200
    When the server provision is committed
      And I send "POST" to "/api/v1/validated" with body
        """
        {"name": "Bob"}
        """
    Then the response code should be 200

  # -----------------------------------------------------------------
  # 8. Server data inspection
  # -----------------------------------------------------------------
  Scenario: Query server data after traffic
    Given a server provision for "POST" on "/api/v1/items"
      And with response code 201
    When the server provision is committed
      And I send "POST" to "/api/v1/items" with body
        """
        {"item": "first"}
        """
      And I send "POST" to "/api/v1/items" with body
        """
        {"item": "second"}
        """
    Then server data for "POST" "/api/v1/items" should have 2 event(s)

  # -----------------------------------------------------------------
  # 9. Logging
  # -----------------------------------------------------------------
  Scenario: Change logging level
    Given logging level is "Error"
    Then logging level should be "Error"
    Given logging level is "Warning"

  # -----------------------------------------------------------------
  # 10. Health
  # -----------------------------------------------------------------
  Scenario: Health
    Then h2agent should be healthy

  # -----------------------------------------------------------------
  # 11. Unused provisions
  # -----------------------------------------------------------------
  Scenario: Detect unused provisions
    Given a server provision for "DELETE" on "/api/v1/never-called"
      And with response code 204
    When the server provision is committed
    Then there should be 1 unused server provision(s)

  # -----------------------------------------------------------------
  # 12. Generic admin escape hatch
  # -----------------------------------------------------------------
  @no_clean
  Scenario: Raw admin call
    When I POST to admin "server-provision"
      """
      {"requestMethod": "GET", "requestUri": "/raw", "responseCode": 200}
      """
    Then the response code should be 201
    When I DELETE admin "server-provision"
    Then the response code should be 200
