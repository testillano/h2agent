Everytime we request a GET to the server with uri '/divide/by/two',
we will calculate a recursive progression ruled by: a(n) = a(n-1)/2,
and answer the result in response body. The previous term corresponds
to the previous request, and the evolving result shall converge to zero.

Response shall be a json document with two fields:
{
  "previous": "<previous result>",
  "current": "<current result: previous/2>"
}

So, provision must indicate proper content-type header in response.

Traffic classification (matching configuration) will be simple, FullMatching,
 as the example is done for an static/constant and hence, predictable URI.

Traffic behavior (provision configuration) will use 'serverEvent' source to
access the last response result (number=-1).
In case of first request, a(n=0)=100.
