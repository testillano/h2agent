1. provision `'(/ctrl/v2/items/update/id-)([0-9]+)'` for POST.

2. provision `'(/ctrl/v2/items/id-)([0-9]+)'` for GET.

   Include transformations for:
   2.1. Capture the URI id into `'uri_parts.2'` variable.

   2.2. Address the POST event with:
      * `requestMethod` POST
      * `requestUri` the POST one with the corresponding id
      * `eventNumber` the last one (-1)
      * `eventPath` as a json pointer path: '/requestBody', transfers the event into the response body at root.
