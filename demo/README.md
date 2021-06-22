# HTTP/2 Server Mock Demo

## Case of use

Our use case consists in a corporate office with a series of workplaces that can be assigned to an employee or be empty. Each position has an *ID* and a 5-digit telephone extension.
We will simulate the server serving GET requests with URI `office/v2/workplace?id=<id>`, obtaining the information associated to the workplace: *id* itself, telephone number, employee complete name or `unassigned`, and date/time when the query have been done.

There are three non-telework employees currently:

```{
id = id-1
phone = 66453
name = Jess Glynne
______________________________
id = id-2
phone = 55643
name = Bryan Adams
______________________________
id = id-3
phone = 32459
name = Phil Collins
```

So, we will respond with the corresponding register except if the *id* is not in the database, when we will generate a document with the *id* provided, a random phone extension (between 30000 and 69999), and the name '*unassigned*'.

As still we can't update provisions through transformation filters, we will ignore the fact that subsequent queries won't be coherent with regarding the phone number for a unassigned register. Also, we can't simulate deletion of registers and expect them to be missing after that operation.

What we can do, is validate the workplace identifiers to be named as `id-<2-digit number>`. If this is not true, an status code 400 (Bad Request) will be answered, with a response body explaining the reason: "invalid workplace id provided, must be in format id-<2-digit number>".

Also, workplaces with even identifiers are designed for developers and an extra field must be included in the response body root indicating this situation: `"developer": true`. This is manually configured for `id-2` but automated through condition variable for unassigned ones.

As we need to identify bad *URI's*, we will use the matching algorithm *PriorityMatchingRegex*, following the next provision sequence:

```json
{
  "requestMethod": "GET",
  "requestUri": "\\/office\\/v2\\/workplace\\?id=id-1",
  "responseCode": 200,
  "responseBody": {
    "id": "id-1",
    "phone": 66453,
    "name": "Jess Glynne"
  },
  "responseHeaders": {
    "content-type": "application/json"
  },
  "transform": [
    {
      "source": "general.strftime.%F %H:%M:%S",
      "target": "response.body.string.time"
    }
  ]
}
```

```json
{
  "requestMethod": "GET",
  "requestUri": "\\/office\\/v2\\/workplace\\?id=id-2",
  "responseCode": 200,
  "responseBody": {
    "id": "id-2",
    "phone": 55643,
    "name": "Bryan Adams",
    "developer": true
  },
  "responseHeaders": {
    "content-type": "application/json"
  },
  "transform": [
    {
      "source": "general.strftime.%F %H:%M:%S",
      "target": "response.body.string.time"
    }
  ]
}
```

```json
{
  "requestMethod": "GET",
  "requestUri": "\\/office\\/v2\\/workplace\\?id=id-3",
  "responseCode": 200,
  "responseBody": {
    "id": "id-3",
    "phone": 32459,
    "name": "Phil Collins"
  },
  "responseHeaders": {
    "content-type": "application/json"
  },
  "transform": [
    {
      "source": "general.strftime.%F %H:%M:%S",
      "target": "response.body.string.time"
    }
  ]
}
```

```json
{
  "requestMethod": "GET",
  "requestUri": "\\/office\\/v2\\/workplace\\?id=id-[0-9]{1,2}",
  "responseCode": 200,
  "responseBody": {
    "name": "unassigned"
  },
  "responseHeaders": {
    "content-type": "application/json"
  },
  "transform": [
    {
      "source": "request.uri.param.id",
      "target": "response.body.string.id"
    },
    {
      "source": "general.random.30000.69999",
      "target": "response.body.integer.phone"
    },
    {
      "source": "general.strftime.%F %H:%M:%S",
      "target": "response.body.string.time"
    },
    {
      "source": "request.uri.param.id",
      "target": "var.isDeveloper",
      "filter": { "RegexCapture" : "id-[0-9]{0,1}[02468]{1}" }
    },
    {
      "source": "value.non-empty-string-is-true",
      "target": "response.body.boolean.developer",
      "filter": { "ConditionVar" : "isDeveloper" }
    }
  ]
}
```

Note that *URI's* are regular expressions, and slashes and question marks **MUST BE ESCAPED**, so you can't put the specified *URI's* directly.

This way is also valid: `"requestUri": "(/office/v2/workplace)\\?id=id-1"`.

And finally, we will configure the default provision:

```json
{
  "requestMethod": "GET",
  "responseCode": 400,
  "responseBody": {
    "cause": "invalid workplace id provided, must be in format id-<2-digit number>"
  },
  "responseHeaders": {
    "content-type": "application/json"
  }
}
```



Additionally we will use the **out-state for foreign method** feature to simulate the deletion of an specific register, for example the *id-2*: we will provision the *DELETE* for it, with an out-state for *GET*, and then we will provision a *404* for such *GET*:

```json
{
  "requestMethod": "DELETE",
  "requestUri": "\\/office\\/v2\\/workplace\\?id=id-2",
  "responseCode": 200,
  "transform": [
    {
      "source": "value.id-2-deleted",
      "target": "outState.GET"
    }
  ]
}
```

```json
{
  "inState": "id-2-deleted",
  "outState": "id-2-deleted",
  "requestMethod": "GET",
  "requestUri": "\\/office\\/v2\\/workplace\\?id=id-2",
  "responseCode": 404
}
```

## Run the demo

Execute the script `./run.sh` once the process has been started.

