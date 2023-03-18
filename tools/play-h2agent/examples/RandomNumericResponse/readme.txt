Everytime we request a GET to the server with uri '/give/me/random/number',
a random number is answered within response body.

Traffic classification (matching configuration) will be simple, FullMatching,
 as the example is done for an static/constant and hence, predictable URI.

Traffic behavior (provision configuration) will use 'random' source between
0 and 999 and set the target over body response as string.
