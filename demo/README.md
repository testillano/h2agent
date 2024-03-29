# H2Agent Demo

## Prepare the environment

Check project [README.md](../README.md#Prepare-the-environment).

## Case of use

Our use case consists in a corporate office with a series of workplaces that can be assigned to an employee or be empty. We will simulate the database having each entry the workplace **id**, a 5-digit **phone** extension, the assigned employee **name** and also an optional root node `developer` to indicate this job role if proceed.

These are the **specifications**:

* Serve *GET* requests with URI `office/v2/workplace?id=<id>`, obtaining the information associated to the workplace. Additionally, the date/time of the query received will be added to the response (`"time"`: `"<free date format>"`). We will provision the following entries:

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

* If requested *id* is missing, we will return a document with the unknown *id* requested, a random phone extension between 30000 and 69999, and the employee name `unassigned`. We will ignore the fact that subsequent queries won't be coherent with regarding the phone number for this unassigned registers (assumed just for simplification).

* Identifiers must be validated as `id-<2-digit number>`. If this is not true, an status code *400 (Bad Request)* will be answered, with this response body explaining the reason:

  ```json
  {
    "cause": "invalid workplace id provided, must be in format id-<2-digit number>"
  }
  ```

* Workplaces with **even** identifiers are reserved for developers, so the extra field commented above must be included in the response body root. This could be manually configured for `id-2` (*Bryan Adams*) but shall be automated for unassigned ones which are even numbers.

* Finally, we will simulate the deletion for existing identifiers (`id-1`, `id-2` and `id-3`), so when *DELETE* request is received for any of them, all subsequent *GET's* must obtain a *404 (Not Found)* status code from then on.

## Analysis

As we need to identify bad *URI's*, we will use the matching algorithm <u>*RegexMatching*</u>, including firstly the known registers, then the "valid but unassigned" ones, and finally a fall back default provision for `GET` method with the *bad request (400)* configuration.

Note that regular expression *URI's*, <u>MUST ESCAPE</u> slashes and question marks:

​	`"\\/office\\/v2\\/workplace\\?id=id-1"`.

This way is also valid:

​	 `(/office/v2/workplace)\\?id=id-1"`.

The optional `developer` node will be implemented through a <u>condition variable transformation filter</u>.

Finally we will use the <u>out-state for foreign method</u> feature to simulate the registers deletion.

## Solution

Check the file [./demo/server-provision.json](./server-provision.json).

For further understanding you may read the project [README.md](https://github.com/testillano/h2agent/blob/master/README.md) file, but we will explain here with more detail the state machine for the last requirement (consistent deletion of registries) because it is an special case of *FSM* (finite state machine) with **foreign methods** (as uses target `outState.GET` within a *DELETE* provision) and it is not normally needed to fulfill most of the testing requirements in the real world because test cases should be restricted to very well known states and limited scopes to have good testing granularity.

Normally `h2agent` is powerful enough using provision in/out states because on regular testing we will mock an specific node and the states are predictable for an specific scenario, that is to say: normally, there is no need to simulate a node in such a deep and reliable way that we do here with deletion use case.

So, focus in the provisions designed for that deletion requirement:

```json
  {
    "requestMethod": "DELETE",
    "requestUri": "\\/office\\/v2\\/workplace\\?id=id-[0-9]{1,2}",
    "responseCode": 204,
    "transform": [
      {
        "source": "value.get-obtains-not-found",
        "target": "outState.GET"
      }
    ],
    "outState": "delete-not-found"
  },
  {
    "inState": "delete-not-found",
    "requestMethod": "DELETE",
    "requestUri": "\\/office\\/v2\\/workplace\\?id=id-[0-9]{1,2}",
    "responseCode": 404,
    "outState": "delete-not-found"
  },
  {
    "requestMethod": "GET",
    "requestUri": "\\/office\\/v2\\/workplace\\?id=id-[0-9]{1,2}",
    "inState": "get-obtains-not-found",
    "outState": "get-obtains-not-found",
    "responseCode": 404
  }
```

*Note*: first item has indeed `inSate: "initial"` (default when this field is missing in the provision object).

Such provisions, in summary say: the first *DELETE* for a valid *URI* will evolve the *DELETE* inner state from "initial" to "delete-not-found". And the second provision defines the behavior for that situation: so, subsequent *DELETES* of the same *URI* will return a *404 (Not Found)*. This is the normal way to play with states and provisions: evolve the state for events related to the same *HTTP* method.

But the first provision says another important thing through its unique transformation item: the state for *GET* requests from then on, will be "get-obtains-not-found". And this is the commented foreign state transition type, which is not so usual, but also useful to simulate complex use cases.

The third provision listed above, defines the way to behave with that new *GET* state: answer *404 (Not Found)* status code.

The key thing to have in mind is the events map, where working states are stored. And it is important to understand that the foreign method transformation generates a virtual event (something that actually never happened through the traffic interface) to force a new state for a supposed *GET*  request in the same *URI* which was deleted: that virtual event is distinguishable thanks to `virtualOrigin`, a node field which could be used as indicator to skip the whole event during test validations.

To better understand, we will show here the snapshots for server data after every operation. Considering for example the case for `id-2`, and omitting the first *GET* (which is done in the demo to show the database entry), we will send the *DELETE*, and then the *GET* to confirm the expected *404 (Not Found)* status code. Finally we will repeat that requests to show that the *FSM* jams on infinite death-way-state for both methods.

Send *DELETE*:

```bash
curl -I -XDELETE --http2-prior-knowledge http://localhost:8000/office/v2/workplace?id=id-2
HTTP/2 204
```

Dump the server data map, just executing the corresponding management interface operation:

`curl --http2-prior-knowledge http://localhost:8074/admin/v1/server-data | jq '.'`

```json
[
  {
    "method": "GET",
    "events": [
      {
        "requestHeaders": {
          "accept": "*/*",
          "user-agent": "curl/7.58.0"
        },
        "previousState": "initial",
        "receptionTimestampUs": 1624564332404138,
        "responseBody": null,
        "responseDelayMs": 0,
        "responseStatusCode": 204,
        "serverSequence": 1,
        "state": "get-obtains-not-found",
        "virtualOrigin": {
           "method": "DELETE",
           "uri": "/office/v2/workplace?id=id-2"
        }
      }
    ],
    "uri": "/office/v2/workplace?id=id-2"
  },
  {
    "method": "DELETE",
    "events": [
      {
        "requestHeaders": {
          "accept": "*/*",
          "user-agent": "curl/7.58.0"
        },
        "previousState": "initial",
        "receptionTimestampUs": 1624564332404199,
        "responseBody": null,
        "responseDelayMs": 0,
        "responseStatusCode": 204,
        "serverSequence": 1,
        "state": "delete-not-found"
      }
    ],
    "uri": "/office/v2/workplace?id=id-2"
  }
]
```

Send *GET*:

```bash
curl -I -XGET --http2-prior-knowledge http://localhost:8000/office/v2/workplace?id=id-2
HTTP/2 404
```

Server data map now:

```json
[
  {
    "method": "GET",
    "events": [
      {
        "requestHeaders": {
          "accept": "*/*",
          "user-agent": "curl/7.58.0"
        },
        "previousState": "initial",
        "receptionTimestampUs": 1624564332404138,
        "responseBody": null,
        "responseDelayMs": 0,
        "responseStatusCode": 204,
        "serverSequence": 1,
        "state": "get-obtains-not-found",
        "virtualOrigin": {
          "method": "DELETE",
          "uri": "/office/v2/workplace?id=id-2"
        }
      },
      {
        "requestHeaders": {
          "accept": "*/*",
          "user-agent": "curl/7.58.0"
        },
        "previousState": "get-obtains-not-found",
        "receptionTimestampUs": 1624564368518135,
        "responseBody": null,
        "responseDelayMs": 0,
        "responseStatusCode": 404,
        "serverSequence": 2,
        "state": "get-obtains-not-found"
      }
    ],
    "uri": "/office/v2/workplace?id=id-2"
  },
  {
    "method": "DELETE",
    "events": [
      {
        "requestHeaders": {
          "accept": "*/*",
          "user-agent": "curl/7.58.0"
        },
        "previousState": "initial",
        "receptionTimestampUs": 1624564332404199,
        "responseBody": null,
        "responseDelayMs": 0,
        "responseStatusCode": 204,
        "serverSequence": 1,
        "state": "delete-not-found"
      }
    ],
    "uri": "/office/v2/workplace?id=id-2"
  }
]
```

Look for `"state"` and `"previousState"` to follow the events, and take into account that the server data is organized in groups of events for specific `method + URI` keys. In the last snapshot, we have two events for *GET* (the first one is virtual), and then one event for *DELETE*. Subsequent events will fill that context dump and we will not show here again:

```bash
curl -I -XDELETE --http2-prior-knowledge http://localhost:8000/office/v2/workplace?id=id-2
HTTP/2 404

curl -I -XGET --http2-prior-knowledge http://localhost:8000/office/v2/workplace?id=id-2
HTTP/2 404

curl -I -XDELETE --http2-prior-knowledge http://localhost:8000/office/v2/workplace?id=id-2
HTTP/2 404

curl -I -XGET --http2-prior-knowledge http://localhost:8000/office/v2/workplace?id=id-2
HTTP/2 404

...
```

## Run the demo

Once `h2agent` process is [started](#Prepare-the-environment), run the demo script (use another terminal if needed):

```bash
$ demo/run.sh
```

The demo script is interactive to follow the use case step by step.

Just in case you want to test demo procedure health, you can execute in non-interactive mode:

```bash
$ INTERACT=false demo/run.sh
$ echo $?
```
