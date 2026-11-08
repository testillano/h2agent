Feature: Library loans mock
  A complete example. The Background is the whole mock: shared patterns and
  presets, the provisions answering the loans API, the lifecycle of every
  loan (one state per loan uri) and a notification sent by the h2agent client
  when a book is returned. The scenarios only play the system under test
  (traffic) and check the outcome, so they read as the business rules.

  API mocked:
    POST   /loans                 lend a book (body: book, membership) -> 201 + location
    GET    /loans/<isbn digits>   the loan, 404 until lent / once returned
    DELETE /loans/<isbn digits>   return the book: 204, then a notification is POSTed
    POST   /notifications         the receiver of those notifications

  Background:
    # --- Shared values: loaded first, referenced by the provisions -------
    Given the vault entry "patterns"
      """
      {"isbn": "([0-9]{13})", "loan": "/loans/([0-9]{13})"}
      """
      And the preset "loan"
      """
      {"library": "central", "renewals": 2, "terms": {"days": 14, "finePerDay": 0.5}}
      """
      And the preset "premium"
      """
      {"days": 30, "finePerDay": 0}
      """
      And the schema "loan-request"
      """
      {"type": "object", "required": ["book"],
       "properties": {"book": {"type": "string", "pattern": "^isbn-[0-9]{13}$"}}}
      """
      And server matching is "RegexMatching"

    # --- Lend: the loan id is the isbn digits ----------------------------
      And the server answers "POST" "/loans" with 201 from preset "loan"
      """
      {}
      """
      And with request schema "loan-request"
      And with transformations
        | from                   | filter                               | to                                   |
        | requestBody /book      | regexReplace '^isbn-#{patterns/isbn}$' $1 | var id                          |
        | requestBody /book      |                                      | responseBody /book                   |
        | literal '/loans/@{id}' |                                      | responseHeader location              |
        | requestBody /membership | equalTo premium                     | var isPremium                        |
        | vault premium          | conditionVar isPremium               | responseBody /terms                  |
        | literal active         |                                      | outState method=GET uri=/loans/@{id} |

    # --- Read: depends on the state of that loan uri ---------------------
      And the server answers "GET" "#{patterns/loan}" with 404
      """
      {"cause": "LOAN_NOT_FOUND"}
      """
      And the server answers "GET" "#{patterns/loan}" with 200 from preset "loan"
      """
      {}
      """
      And in state "active"
      And with transformations
        | from               | filter                               | to                 |
        | requestUriPath     | regexReplace '^#{patterns/loan}$' $1 | responseBody /book |
        | responseBody /book | prepend isbn-                        | responseBody /book |

    # --- Return: closes the loan (always 204) and notifies ---------------
      And the server answers "DELETE" "#{patterns/loan}" with 204
      And with transformations
        | from            | filter                               | to                                   |
        | requestUriPath  | regexReplace '^#{patterns/loan}$' $1 | var id                               |
        | literal initial |                                      | outState method=GET uri=/loans/@{id} |
        | var id          |                                      | vault lastReturned                   |
        | literal go      |                                      | clientProvision notifyReturn initial |

    # --- The notification: the h2agent client towards its own mock -------
      And the server answers "POST" "/notifications" with 204
      And the client endpoint "self" at "localhost" port 8000
      And the client "notifyReturn" sends "POST" "/notifications" to "self"
      """
      {"event": "BOOK_RETURNED"}
      """
      And with transformations
        | from               | to                |
        | vault lastReturned | requestBody /book |

  Scenario Outline: The loan terms depend on the membership
    When I send "POST" to "/loans" with body
      """
      {"book": "isbn-<isbn>", "membership": "<membership>"}
      """
    Then the response code should be 201
      And the response header "location" should be "/loans/<isbn>"
      And the response body at "/book" should be "isbn-<isbn>"
      And the response body at "/library" should be "central"
      And the response body at "/terms/days" should be "<days>"
      And the response body at "/terms/finePerDay" should be "<fine>"

    Examples:
      | isbn          | membership | days | fine |
      | 9780000000001 | basic      | 14   | 0.5  |
      | 9780000000002 | premium    | 30   | 0    |
      | 9780000000003 |            | 14   | 0.5  |

  Scenario: A loan lives from the lending to the return
    When I send "GET" to "/loans/9780000000001"
    Then the response code should be 404
      And the response body at "/cause" should be "LOAN_NOT_FOUND"
    When I send "POST" to "/loans" with body
      """
      {"book": "isbn-9780000000001", "membership": "premium"}
      """
      And I send "GET" to "/loans/9780000000001"
    Then the response code should be 200
      And the response body at "/book" should be "isbn-9780000000001"
    When I send "DELETE" to "/loans/9780000000001"
    Then the response code should be 204
    When I send "GET" to "/loans/9780000000001"
    Then the response code should be 404

  Scenario: Returning a book notifies it
    When I send "POST" to "/loans" with body
      """
      {"book": "isbn-9780000000007"}
      """
      And I send "DELETE" to "/loans/9780000000007"
    Then the server should have received 1 "POST" "/notifications"
      And server data for "POST" "/notifications" event -1 at "/requestBody" should be
      """
      {"event": "BOOK_RETURNED", "book": "9780000000007"}
      """

  Scenario: Loans are independent
    When I send "POST" to "/loans" with body
      """
      {"book": "isbn-9780000000001"}
      """
      And I send "POST" to "/loans" with body
      """
      {"book": "isbn-9780000000002"}
      """
      And I send "DELETE" to "/loans/9780000000002"
      And I send "GET" to "/loans/9780000000001"
    Then the response code should be 200
    When I send "GET" to "/loans/9780000000002"
    Then the response code should be 404

  Scenario: A malformed request is rejected
    When I send "POST" to "/loans" with body
      """
      {"book": "The Hobbit"}
      """
    Then the response code should be 400
      And the server should have received 0 "POST" "/notifications"
