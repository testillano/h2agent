We will capture numeric final part from URI and send its value on response body.
URI will be like this: '/app/v1/foo/bar/<number>'.

Traffic classification (matching configuration) should ignore the final
numeric part to obtain a predictable ouput, so the appropiate algorithm
would be FullMatchingRegexReplace with 'rgx' field as regular expression
to match the received URI: (/app/v1/foo/bar/)([0-9]*), and 'fmt' field as
algorithm output: first captured group '$1'.

Traffic behavior (provision configuration) will be ruled by a provision with
request URI key as the fixed/predictable part '/app/v1/foo/bar/' resulting
from traffic classification. In order to access the ignored numeric part, we
will transform the original URI source (request.uri) using RegexpReplace filter.
