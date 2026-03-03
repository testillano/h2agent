Everytime we request a GET to the server with uri '/give/me/random/calculation',
a random math calculation is presented and solved within response body.

Traffic classification (matching configuration) will be simple, FullMatching,
as the example is done for an static/constant and hence, predictable URI.

Traffic behavior (provision configuration) will use 'random' sources to build
the math expression, for example, given 'a + bx' we will generate a, b and x,
and then calculate the result. To simplify, we will use numbers between 0 and 9.
