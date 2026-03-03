When we request a POST to the server with uri '/validate', we will obtain
a status code 200 if response is validated against a request schema (400
if not): "myRequestSchema".

We additionally will validate with more precission by mean a second schema
using a transformation filter (SchemaId): "myFilterSchema". In this case,
validation error sets the status code 417.

The first schema check fields 'num' and 'name' to be a number and a string
respectively. The second restricts the validation scope: 'num' must be a
number between 0 and 255, and the string must be in lower case.

Traffic classification (matching configuration) will be simple, FullMatching,
as the example is done for an static/constant and hence, predictable URI.

Traffic behavior (provision configuration) will configure a response with the
field 'failReport' which could be empty or not depending on the SchmaId filter
validation result. The request body will be like this:

{
  "num": <random between 0 and 512>,
  "name": "<random string or boolean>"
}
