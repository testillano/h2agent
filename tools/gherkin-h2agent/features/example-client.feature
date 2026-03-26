Feature: h2agent client mode examples
  The client provisions here interact with server provisions
  from example-server.feature. Run the server example first
  or provision the server within the same scenario.

  # -----------------------------------------------------------------
  # 1. Single request — client hits the echo endpoint
  # -----------------------------------------------------------------
  Scenario: Client sends POST to echo endpoint
    # Server side: echo /api/v1/users (same as example-server scenario 2)
    Given server matching is "FullMatching"
      And a server provision for "POST" on "/api/v1/users"
      And with response code 201
      And with response body
        """
        {"id": 1}
        """
      And with transform from "request.body./name" to "response.body.json.string./name"
    When the server provision is committed

    # Client side: send a request and capture the response
    Given a client endpoint "local" at "localhost" port 8000
      And a client provision "createUser"
      And with endpoint "local"
      And with request method "POST"
      And with request uri "/api/v1/users"
      And with request body
        """
        {"name": "Bob"}
        """
      And with on-response transform from "response.body./name" to "vault.lastCreatedUser"
    When the client provision is committed
      And client provision "createUser" is triggered

    Then the response code should be 200
      And vault "lastCreatedUser" should be "Bob"
      And client data for "local" "POST" "/api/v1/users" should have 1 event(s)

  # -----------------------------------------------------------------
  # 2. Sequenced requests — dynamic body per iteration
  # -----------------------------------------------------------------
  Scenario: Client sends multiple requests with sequence in body
    # Server side: accept POSTs, echo back
    Given server matching is "FullMatching"
      And a server provision for "POST" on "/api/v1/items"
      And with response code 201
      And with response body
        """
        {"received": true}
        """
    When the server provision is committed

    # Client side: send 5 requests, each with sequence number in body
    Given a client endpoint "local" at "localhost" port 8000
      And a client provision "batchInsert"
      And with endpoint "local"
      And with request method "POST"
      And with request uri "/api/v1/items"
      And with request body
        """
        {"item": "placeholder"}
        """
      And with transform from "value.item-@{sequence}" to "request.body.json.string./item"
    When the client provision is committed
      And client provision "batchInsert" is triggered from 1 to 5 at 50 cps

    Then the response code should be 202
      And server data for "POST" "/api/v1/items" should have 5 event(s)

  # -----------------------------------------------------------------
  # 3. Client flow with state chain — auth then fetch
  # -----------------------------------------------------------------
  Scenario: Two-step client flow using scoped variables
    # Server side: auth endpoint returns a token
    Given server matching is "FullMatching"
      And a server provision for "POST" on "/api/v1/auth"
      And with response code 200
      And with response body
        """
        {"token": "secret-abc-123"}
        """
    When the server provision is committed

    # Server side: data endpoint echoes the authorization header
    Given a server provision for "GET" on "/api/v1/data"
      And with response code 200
      And with response body
        """
        {"data": "protected-content"}
        """
      And with transform from "request.header.authorization" to "response.body.json.string./auth"
    When the server provision is committed

    # Client step 1: authenticate, capture token, chain to step 2
    Given a client endpoint "local" at "localhost" port 8000
      And a client provision "authenticate"
      And with endpoint "local"
      And with inState "initial"
      And with outState "authenticated"
      And with request method "POST"
      And with request uri "/api/v1/auth"
      And with on-response transform from "response.body./token" to "var.token"
    When the client provision is committed

    # Client step 2: fetch data using the captured token
    Given a client provision "fetchData"
      And with endpoint "local"
      And with inState "authenticated"
      And with request method "GET"
      And with request uri "/api/v1/data"
      And with transform from "var.token" to "request.header.authorization"
    When the client provision is committed

    # Trigger the chain — step 1 fires, state progresses, step 2 fires
    When client provision "authenticate" is triggered
    Then the response code should be 200
      And client data for "local" "POST" "/api/v1/auth" should have 1 event(s)
      And client data for "local" "GET" "/api/v1/data" should have 1 event(s)

  # -----------------------------------------------------------------
  # 4. JSON escape hatch for client provision
  # -----------------------------------------------------------------
  Scenario: Client provision via full JSON
    Given server matching is "FullMatching"
      And a server provision for "GET" on "/api/v1/status"
      And with response code 200
      And with response body
        """
        {"status": "ok", "version": "1.0"}
        """
    When the server provision is committed

    Given a client endpoint "local" at "localhost" port 8000
      And client provision
        """
        {
          "id": "checkStatus",
          "endpoint": "local",
          "requestMethod": "GET",
          "requestUri": "/api/v1/status",
          "onResponseTransform": [
            {"source": "response.body./status", "target": "vault.svcStatus"},
            {"source": "response.body./version", "target": "vault.svcVersion"}
          ]
        }
        """
    When client provision "checkStatus" is triggered
    Then the response code should be 200
      And vault "svcStatus" should be "ok"
      And vault "svcVersion" should be "1.0"
