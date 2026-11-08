Feature: h2agent as a client
  The h2agent client sends requests to a server; here that server is the
  same h2agent's mock server (endpoint "self"), so the features run alone.
  A trigger starts a flow which runs asynchronously: the Then steps wait for
  the expected result (up to 3 seconds; behave -D timeout=<s> to change).

  Background:
    Given server matching is "FullMatching"
      And the client endpoint "self" at "localhost" port 8000

  Scenario: Send one request and keep a value of the answer
    Given the server answers "POST" "/api/v1/users" with 201
      """
      {"id": 1}
      """
      And with transformations
        | from              | to                 |
        | requestBody /name | responseBody /name |
      And the client "createUser" sends "POST" "/api/v1/users" to "self"
      """
      {"name": "Bob"}
      """
      And with response transformations
        | from               | to                   |
        | responseBody /name | vault lastCreatedUser |
    When the client "createUser" is triggered
    Then the client "createUser" should receive 201
      And the client "createUser" response body at "/name" should be "Bob"
      And the vault entry "lastCreatedUser" should be "Bob"

  Scenario: A sequence of requests with a body built per request
    Given the server answers "POST" "/api/v1/items" with 201
      And the client "batchInsert" sends "POST" "/api/v1/items" to "self"
      """
      {"item": ""}
      """
      And with transformations
        | from                   | to                 |
        | literal 'item-@{sequence}'| requestBody /item  |
    When client provision "batchInsert" is triggered from 1 to 5 at 50 cps
    Then the server should have received 5 "POST" "/api/v1/items"
      And server data for "POST" "/api/v1/items" event -1 at "/requestBody/item" should be "item-5"

  Scenario: Two chained requests sharing a value
    Given the server answers "POST" "/api/v1/auth" with 200
      """
      {"token": "secret-abc-123"}
      """
      And the server answers "GET" "/api/v1/data" with 200
      """
      {"data": "protected-content"}
      """
      And with transformations
        | from                        | to                 |
        | requestHeader authorization | responseBody /auth |
      And the client "authenticate" sends "POST" "/api/v1/auth" to "self"
      And in state "initial" moving to "authenticated"
      And with response transformations
        | from                | to        |
        | responseBody /token | var token |
      And the client "authenticate" sends "GET" "/api/v1/data" to "self"
      And in state "authenticated"
      And with transformations
        | from      | to                          |
        | var token | requestHeader authorization |
    When the client "authenticate" is triggered
    Then the server should have received 1 "POST" "/api/v1/auth"
      And the server should have received 1 "GET" "/api/v1/data"
      And the client "authenticate" response body at "/auth" should be "secret-abc-123"

  Scenario: Request bodies from presets
    Given the preset "member"
      """
      {"membership": "basic", "city": "Madrid"}
      """
      And the server answers "POST" "/api/v1/members" with 201
      And the client "addMember" sends "POST" "/api/v1/members" to "self" from preset "member"
      """
      {"name": "Ada"}
      """
    When the client "addMember" is triggered
    Then the client "addMember" should receive 201
      And server data for "POST" "/api/v1/members" event 1 at "/requestBody" should be
      """
      {"membership": "basic", "city": "Madrid", "name": "Ada"}
      """
