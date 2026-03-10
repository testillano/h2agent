# H2Agent Management API - User Guide

This guide covers the concepts, behavior and usage patterns of the h2agent management API.
For the endpoint reference (parameters, schemas, status codes), see the [OpenAPI specification](./openapi.yaml)
or the [interactive documentation](https://testillano.github.io/h2agent/api/).

## Table of contents

- [Overview](#overview)
- [General operations](#general-operations)
  - [Schemas](#schemas)
  - [Global variables](#global-variables)
  - [Files](#files)
  - [Logging](#logging)
  - [Configuration](#configuration)
- [Matching algorithms](#matching-algorithms)
  - [URI path query parameters](#uri-path-query-parameters)
  - [Regex configuration (rgx & fmt)](#regex-configuration-rgx--fmt)
  - [Algorithm selection](#algorithm-selection)
- [Server provisions](#server-provisions)
  - [State machine (inState/outState)](#state-machine-instateoutstate)
  - [Server provision fields](#server-provision-fields)
  - [Transformation pipeline](#transformation-pipeline)
  - [Multiple provisions](#multiple-provisions)
  - [Unused provisions](#unused-provisions)
- [Server data](#server-data)
  - [Storage configuration](#storage-configuration)
  - [Querying server data](#querying-server-data)
  - [Server data summary](#server-data-summary)
  - [Deleting server data](#deleting-server-data)
- [Client endpoints](#client-endpoints)
- [Client provisions](#client-provisions)
  - [Client state machine](#client-state-machine)
  - [Client provision fields](#client-provision-fields)
  - [Client transformation differences](#client-transformation-differences)
  - [Multiple client provisions](#multiple-client-provisions)
  - [Unused client provisions](#unused-client-provisions)
  - [Triggering](#triggering)
    - [Partial chain execution](#partial-chain-execution)
    - [Server-triggered client flows](#server-triggered-client-flows)
- [Client data](#client-data)
  - [Client storage configuration](#client-storage-configuration)
  - [Querying client data](#querying-client-data)
  - [Client data summary](#client-data-summary)
  - [Deleting client data](#deleting-client-data)

## Overview

`h2agent` listens on a specific management port (*8074* by default) for incoming requests, implementing a *REST API* to manage the process operation. Through the *API* we could program the agent behavior. All operations are served over *URI* path `/admin/v1/`.

The API is organized in three groups:

**General** mock operations:

* Schemas: define validation schemas used in further provisions to check the incoming and outgoing traffic.
* Global variables: shared variables between different provision contexts and flows. Extra feature to solve some situations by other means, and also used for built-in response condition mechanism: [Dynamic response delays](../../README.md#dynamic-response-delays).
* Logging: dynamic logger configuration (update and check).
* General configuration (server).

**Traffic server mock** features:

* Server matching configuration: classification algorithms to split the incoming traffic and access to the final procedure which will be applied.
* Server provision configuration: here we will define the mock behavior regarding the request received, and the transformations done over it to build the final response and evolve, if proceed, to another state for further receptions.
* Server data storage: data inspection is useful for both external queries (mainly troubleshooting) and internal ones (provision transformations). Also storage configuration will be described.

**Traffic client mock** features:

* Client endpoints configuration: remote server addresses configured to be used by client provisions.
* Client provision configuration: here we will define the mock behavior regarding the request sent, and the transformations done over it to build the final request and evolve, if proceed, to another flow for further sendings.
* Client data storage: data inspection is useful for both external queries (mainly troubleshooting) and internal ones (provision transformations). Also storage configuration will be described.

## General operations

### Schemas

Loads schema(s) (`POST /admin/v1/schema`) for future event validation. Added schemas could be referenced within provision configurations by mean their string identifier.

If you have a `json` schema (from file `schema.json`) and want to build the `h2agent` schema configuration (into file `h2agent_schema.json`), you may perform automations like this *bash script* example:

```bash
$ jq --arg id "theSchemaId" '. | { id: $id, schema: . }' schema.json > h2agent_schema.json
```

Also *python* or any other language could do the job:

```python
>>> schema = {"$schema":"http://json-schema.org/draft-07/schema#","type":"object","additionalProperties":True,"properties":{"foo":{"type":"string"}},"required":["foo"]}
>>> print({ "id":"theSchemaId", "schema":schema })
```

Load of a set of schemas through an array object is allowed. So, instead of launching *N* schema loads separately, you could group them:

```json
[
  {
    "id": "myRequestsSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "foo": { "type": "string" }
      },
      "required": [ "foo" ]
    }
  },
  {
    "id": "myResponsesSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "bar": { "type": "number" }
      },
      "required": [ "bar" ]
    }
  }
]
```

A schema set fails with the first failed item, giving a 'pluralized' version of the single load failed response message.

### Global variables

Global variables (`POST /admin/v1/global-variable`) can be created dynamically from provisions execution (to be used there in later transformations steps or from any other different provision, due to the global scope), but they also can be loaded through this *REST API* operation. This operation is mainly focused on the use of global variables as constants for the whole execution (although they could be updated or reset from provisions).

Global variables are created as string-value, which will be interpreted as numbers or any other data type, depending on the transformation involved.

When querying (`GET /admin/v1/global-variable`), without the `name` query parameter, the whole list of global variables is returned as a JSON object. With the `name` parameter, the specific variable value is returned as a plain string. A variable used as memory bucket could store even binary data and it may be obtained with this operation.

### Files

The files operation (`GET /admin/v1/files`) retrieves the whole list of files processed and their current status. For example:

```json
[
  {
    "bytes": 1791,
    "closeDelayUsecs": 1000000,
    "path": "Mozart.txt",
    "state": "closed"
  },
  {
    "bytes": 1770,
    "closeDelayUsecs": 1000000,
    "path": "Beethoven.txt",
    "state": "opened"
  }
]
```

An already managed file could be externally removed or corrupted. In that case, the state "missing" will be shown.

### Logging

The logging level (`GET /admin/v1/logging`) retrieves the current level: `Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency`.

To change it (`PUT /admin/v1/logging?level=<level>`), provide any of the valid log levels as query parameter. This can be also configured on start as described in the [command line](../../README.md#command-line) section.

### Configuration

The general process configuration (`GET /admin/v1/configuration`) returns:

```json
{
    "longTermFilesCloseDelayUsecs": 1000000,
    "shortTermFilesCloseDelayUsecs": 0,
    "lazyClientConnection": true
}
```

The server configuration (`GET /admin/v1/server/configuration`) returns:

```json
{
    "preReserveRequestBody": true,
    "receiveRequestBody": true
}
```

Request body reception can be disabled (`PUT /admin/v1/server/configuration`). This is useful to optimize the server processing in case that request body content is not actually needed by planned provisions:

* `receiveRequestBody=true`: data received will be processed.
* `receiveRequestBody=false`: data received will be ignored.

The `h2agent` starts with request body reception enabled by default, but you could also disable this through command-line (`--traffic-server-ignore-request-body`).

Also, request body memory pre-reservation could be disabled to be dynamic. This simplifies the model behind (`http2comm` library) disabling the default optimization which minimizes reallocations done when data chunks are processed:

* `preReserveRequestBody=true`: pre reserves memory for the expected request body (with the maximum message size received in a given moment).
* `preReserveRequestBody=false`: allocates memory dynamically during append operations for data chunks processed.

The `h2agent` starts with memory pre reservation enabled by default, but you could also disable this through command-line (`--traffic-server-dynamic-request-body-allocation`).

## Matching algorithms

The server matching configuration (`POST /admin/v1/server-matching`) defines how incoming traffic is classified towards provisions.

### URI path query parameters

Optional object used to specify the transformation used for traffic classification, of query parameters received in the *URI* path. It contains two fields, a mandatory _filter_ and an optional _separator_:

* *filter*:
  * *Sort*: this is the <u>default behavior</u>, which sorts, if received, query parameters to make provisions predictable for unordered inputs.
  * *PassBy*: if received, query parameters are used to classify without modifying the received *URI* path (query parameters are kept as received).
  * *Ignore*: if received, query parameters are ignored during classification (removed from *URI* path and not taken into account to match provisions). Note that query parameters are stored to be accessible on provision transformations, because this filter is only considered to classify traffic.
* *separator*:
  * *Ampersand*: this is the <u>default behavior</u> (if whole _uriPathQueryParameters_ object is not configured) and consists in split received query parameters keys using *ampersand* (`'&'`) as separator for key-value pairs.
  * *Semicolon*: using *semicolon* (`';'`) as query parameters pairs separator is rare but still applies on older systems.

### Regex configuration (rgx & fmt)

Optional arguments used in `FullMatchingRegexReplace` algorithm.
Regular expressions used by `h2agent` are based on `std::regex` and built with default `ECMAScript` option type from [available](https://en.cppreference.com/w/cpp/regex/syntax_option_type) ones.
Also, `std::regex_replace` and `std::regex_match` algorithms use default format (`ECMAScript` [rules](https://262.ecma-international.org/5.1/#sec-15.5.4.11)) from [available](https://en.cppreference.com/w/cpp/regex/match_flag_type) ones.

### Algorithm selection

There are three classification algorithms. Two of them classify the traffic by single identification of the provision key (`method`, `uri` and `inState`): `FullMatching` matches directly, and `FullMatchingRegexReplace` matches directly after transformation. The other one, `RegexMatching` is not matching by identification but for regular expression.

In summary:

* `FullMatching`: when <u>all</u> the *method/URIs* are completely <u>predictable</u>.
* `FullMatchingRegexReplace`: when <u>some</u> *URIs* should be <u>transformed</u> to get <u>predictable</u> ones (for example, timestamps trimming, variables in path or query parameters, etc.), and <u>other</u> *URIs* are disjoint with them, but also <u>predictable</u>.
* `RegexMatching`: when we have a <u>mix</u> of <u>unpredictable</u> *URIs* in our test plan.

#### FullMatching

Arguments `rgx` and `fmt` are not used here, so not allowed. The incoming request is fully translated into key without any manipulation, and then searched in internal provision map.

This is the default algorithm. Internal provision is stored in a map indexed with real requests information to compose an aggregated key (normally containing the requests *method* and *URI*, but as future proof, we could add `expected request` fingerprint). Then, when a request is received, the map key is calculated and retrieved directly to be processed.

This algorithm is very good and easy to use for predictable functional tests (as it is accurate), also giving internally better performance for provision selection.

#### FullMatchingRegexReplace

Both `rgx` and `fmt` arguments are required. This algorithm is based in [regex-replace](http://www.cplusplus.com/reference/regex/regex_replace/) transformation. The first one (*rgx*) is the matching regular expression, and the second one (*fmt*) is the format specifier string which defines the transformation. Previous *full matching* algorithm could be simulated here using empty strings for `rgx` and `fmt`, but having obviously a performance degradation due to the filter step.

For example, you could trim an *URI* received in different ways:

`URI` example:

```
uri = "/ctrl/v2/id-555112233/ts-1615562841"
```

* Remove last *timestamp* path part (`/ctrl/v2/id-555112233`):

```
rgx = "(/ctrl/v2/id-[0-9]+)/(ts-[0-9]+)"
fmt = "$1"
```

* Trim last four digits (`/ctrl/v2/id-555112233/ts-161556`):

```
rgx = "(/ctrl/v2/id-[0-9]+/ts-[0-9]+)[0-9]{4}"
fmt = "$1"
```

So, this `regex-replace` algorithm is flexible enough to cover many possibilities (even *tokenize* path query parameters, because <u>the whole received `uri` is processed</u>, including that part). As future proof, other fields could be added, like algorithm flags defined in underlying C++ `regex` standard library used.

Also, `regex-replace` could act as a virtual *full matching* algorithm when the transformation fails (the result will be the original tested key), because it can be used as a <u>fall back to cover non-strictly matched receptions</u>. The limitation here is when those unmatched receptions have variable parts (it is impossible/unpractical to provision all the possibilities). So, this fall back has sense to provision constant reception keys (fixed and predictable *URIs*), and of course, strict provision keys matching the result of `regex-replace` transformation on their reception keys which does not fit the other fall back ones.

#### RegexMatching

Arguments `rgx` and `fmt` are not used here, so not allowed. Provision keys are in this case, regular expressions to match reception keys. As we cannot search the real key in the provision map, we must check the reception sequentially against the list of regular expressions, and this is done assuming the first match as the valid one. So, this identification algorithm relies in the configured provision order to match the receptions and select the first valid occurrence.

This algorithm allows to provision with priority. For example, consider 3 provision operations which are provided sequentially in the following order:

1. `/ctrl/v2/id-55500[0-9]{4}/ts-[0-9]{10}`
2. `/ctrl/v2/id-5551122[0-9]{2}/ts-[0-9]{10}`
3. `/ctrl/v2/id-555112244/ts-[0-9]{10}`

If the `URI` "*/ctrl/v2/id-555112244/ts-1615562841*" is received, the second one is the first positive match and then, selected to mock the provisioned answer. Even being the third one more accurate, this algorithm establish an ordered priority to match the information.

Note: in case of large provisions, this algorithm could be not recommended (sequential iteration through provision keys is slower that map search performed in *full matching* procedures).


## Server provisions

Server provisions (`POST /admin/v1/server-provision`) define the response behavior for incoming requests. This section covers the conceptual aspects of provisioning.

### State machine (inState/outState)

We could label a provision specification to take advantage of internal *FSM* (finite state machine) for matched occurrences. When a reception matches a provision specification, the real context is searched internally to get the current state ("**initial**" if missing or empty string provided) and then get the `inState` provision for that value. Then, the specific provision is processed and the new state will get the `outState` provided value. This makes possible to program complex flows which depends on some conditions, not only related to matching keys, but also consequence from [transformation filters](#filters) which could manipulate those states.

These arguments are configured by default with the label "**initial**", used by the system when a reception does not match any internal occurrence (as the internal state is unassigned). This conforms a default rotation for further occurrences because the `outState` is again the next `inState` value. It is important to understand that if there is not at least 1 provision with `inState` = "**initial**" the matched occurrences won't never be processed. Also, if the next state configured (`outState` provisioned or transformed) has not a corresponding `inState` value, the flow will be broken/stopped.

So, "**initial**" is a reserved value which is mandatory to debut any kind of provisioned transaction. Remember that an empty string will be also converted to this special state for both `inState` and `outState` fields, and character `#` is not allowed (check [this](../../docs/developers/AggregatedKeys.md) document for developers).

#### Example

* Provision *X* (match *m*, `inState`="*initial*"): `outState`="*second*", `response` *XX*
* Provision *Y* (match *m*, `inState`="*second*"): `outState`="*initial*", `response` *YY*
* Reception matches *m* and internal context map (server data) is empty: as we assume state "*initial*", we look for this `inState` value for match *m*, which is provision *X*.
* Response *XX* is sent. Internal state will take the provision *X* `outState`, which is "*second*".
* Reception matches *m* and internal context map stores state "*second*", we look for this `inState` value for match *m*, which is provision Y.
* Response *YY* is sent. Internal state will take the provision *Y* `outState`, which is "*initial*".

Further similar matches (*m*), will repeat the cycle again and again.

<u>Important note</u>: match *m* refers to matching key, that is to say: provision `method` and `uri`, but states are linked to real *URIs* received (coincide with match key `uri` for *FullMatching* classification algorithm, but not for others). So, there is a different state machine definition for each specific provision and so, a different current state for each specific events fulfilling such provision (this is much better that limiting the whole mock configuration with a global *FSM*, as for example, some events could fail due to *SUT* bugs and states would evolve different for their corresponding keys). If your mock receives several requests with different *URIs* for an specific test stage name, consider to name their provision states with the same identifier (with the stage name, for example), because different provisions will evolve at the "same time" and those names does not collide because they are different state machines (different matches). This could ease the flow understanding as those requests are received in a known test stage.

#### Special purge state

Stateful scenarios normally require access to former events (available at server data storage) to evolve through different provisions, so disabling server data is not an option to make them work properly. The thing is that high load testing could impact on memory consumption of the mock server if we don't have a way to clean information which is no longer needed and could be dangerously accumulated. Here is where purge operation gets importance: the keyword '*purge*' is a reserved out-state used to indicate that server data related to an event history must be dropped (it should be configured at the last scenario stage provision). This mechanism is useful in long-term load tests to avoid the commented high memory consumption removing those scenarios which have been successfully completed. A nice side-effect of this design, is that all the failed scenarios will be available for further analysis, as purge operation is performed at last scenario stage and won't be reached normally in this case of fail.

### Server provision fields

#### requestMethod

Expected request method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*).

The modern `HTTP` specification (RFC 9110) states that all general-purpose `HTTP` servers must implement at least the `GET` and `HEAD` methods. The implementation of `HEAD` must be equivalent to `GET` for the same resource, with the sole and crucial difference that the `HEAD` response must never contain a message body (entity). However, the "mock server" mode of this application allows simulating `HEAD` responses that are incorrect or do not follow the specification. In fact, by default, the `HEAD` response corresponding to a `GET` provision configured is not automatically implemented; instead, the user must actively do this when setting up the mock.

#### requestUri

Request *URI* path (percent-encoded) to match depending on the algorithm selected. It includes possible query parameters, depending on matching filters provided for them.

<u>*Empty string is accepted*</u>, and is reserved to configure an optional default provision, something which could be specially useful to define the fall back provision if no matching entry is found. So, you could configure defaults for each method, just putting an empty *request URI* or omitting this optional field. Default provisions could evolve through states (in/out) but at least "initial" is again mandatory to be processed.

#### requestSchemaId

We could optionally validate requests against a `json` schema. Schemas are identified by string name and configured through [command line](../../README.md#command-line) or REST API. When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

#### responseHeaders

Header fields for the response. For example:

```json
"responseHeaders":
{
  "content-type": "application/json"
}
```

#### responseCode

Response status code. It can also be used to transport `RST_STREAM` and `GOAWAY` [error codes](https://datatracker.ietf.org/doc/html/rfc7540#section-7):

| Hex value | Name | Description |
|-----------|------|-------------|
| 0x0 | NO_ERROR | Normal graceful close GOAWAY |
| 0x1 | PROTOCOL_ERROR | Protocol error at frame level |
| 0x2 | INTERNAL_ERROR | Internal error |
| 0x3 | FLOW_CONTROL_ERROR | Flow control error |
| 0x7 | REFUSED_STREAM | Frame rejected before processing |
| 0x8 | CANCEL | Stream cancelled by application |
| 0xB | STREAM_CLOSED | Reception of frame already closed |

Those values are also defined in `nghttp2_error_code` enum type within `https://nghttp2.org/documentation/nghttp2.h.html`.

Any value lesser than 100 will cancel the ongoing stream with the corresponding error value. For values greater or equal than 100, it will be interpreted as `HTTP Status Codes`.

Error codes are registered as global variables with key `'__core.stream-error-traffic-server.<recvseq>.<method>.<uri>'` and value `'<error code>'`.

#### responseBody

Response body. Currently supported: object (`json` and arrays), string, integer, number, boolean and null types.

#### responseDelayMs

Optional response delay simulation in milliseconds.

#### responseSchemaId

We could optionally validate built responses against a `json` schema. When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.
### Transformation pipeline

Sorted list of transformation items to modify incoming information and build the dynamic response to be sent.

Each transformation has a `source`, a `target` and an optional `filter` algorithm. <u>The filters are applied over sources and sent to targets</u> (all the available filters at the moment act over sources in string format, so they need to be converted if they are not strings in origin).

A best effort is done to transform and convert information to final target vaults, and when something wrong happens, a logging error is thrown and the transformation filter is skipped going to the next one to be processed. For example, a source detected as *json* object cannot be assigned to a number or string target, but could be set into another *json* object.

Let's start describing the available sources of data: regardless the native or normal representation for every kind of target, the fact is that conversions may be done to almost every other type:

- *string* to *number* and *boolean* (true if non empty).

- *number* to *string* and *boolean* (true if different than zero).

- *boolean*: there is no source for boolean type, but you could create a non-empty string or non-zeroed number to represent *true* on a boolean target (only response body nodes could include a boolean).

- *json object*: request body node (whole document is indeed the root node) when being an object itself (note that it may be also a number or string). This data type can only be transfered into targets which support json objects like response body json node.



*Variables substitution:*

Before describing sources and targets (and filters), just to clarify that in some situations it is allowed the insertion of variables in the form `@{var id}` which will be replaced if exist, by scoped provision variables and global variables. In that case we will add the comment "**admits variables substitution**". At certain sources and targets, substitutions are not allowed because have no sense or they are rarely needed:



The **source** of information is classified after parsing the following possible expressions:

- request.uri: whole `url-decoded` and **normalized** request *URI* (path together with possible query parameters **sorted**). Not necessarily the same as the classification *URI* (which could ignore those query parameters, or even pass them by from *URI* with no order guaranteed) and in the same way, not necessarily the same as the original *URI* due to the same reason: query parameters order. Normalization makes the source more predictable, something useful to extract specific *URI* parts. For example, consider the *URI* `/composer?city=Bonn&author=Beethoven`, which normalized would turn into `/composer?author=Beethoven&city=Bonn` (because query parameters are sorted). Assuming that source format, you may use the regular expression `(/composer\?author=)([a-zA-Z]*)(&city=)([a-zA-Z]*)`, to extract the author name or city from that predictable normalized *URI* (second or fourth capture group) . This kind of transformation is very usual regardless if query parameters are processed or not.

- request.uri.path: `url-decoded` request *URI* path part.

- request.uri.param.`<name>`: request URI specific parameter `<name>`.

- request.body: request body received. Should be interpreted depending on the request content type. In case of `json`, it will be the document from *root*. In case of `multipart` reception, a proprietary `json` structure is built to ease accessibility, for example:

  ```json
  {
    "multipart.1": {
      "content": {
        "foo": "bar"
      },
      "headers": {
        "Content-Type": "application/json"
      }
    },
    "multipart.2": {
      "content": "0x268aff26",
      "headers": {
        "Content-Type": "application/octet-stream"
      }
    }
  }
  ```

  So, every part is labeled as `multipart.<number>` with nested `content` and `headers` (the content representation depends again on the content type received in the nested headers field). The drawback for `multipart` reception is that we cannot access the original raw data through this `request.body` source because it is transformed into `json` nature as an usability assumption. Anyway, proprietary structure is more useful and probable to be needed, so future proof for raw access is less priority.

- request.body.`/<node1>/../<nodeN>`: request body node `json` path. This source path **admits variables substitution**. Leading slash is needed as first node is considered the `json` Also, `multipart` content can be accessed to retrieve any of the nested parts in the proprietary `json` representation commented above.

- response.body: response body as template. Should be interpreted depending on the response content type. The use of provisioned response as template reference is rare but could ease the build of structures for further transformations, In case of `json` it will be the document from *root*.

  As the transformation steps modify this data container, its value as a source is likewise updated.

- response.body.`/<node1>/../<nodeN>`: response body node `json` path. This source path **admits variables substitution**. The use of provisioned response as template reference is rare but could ease the build of `json` structures for further transformations.

  As the transformation steps modify this data container, its value as a source is likewise updated.

- request.header.`<hname>`: request header component (i.e. *content-type*). Take into account that header fields values are received [lower cased](https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2).

- eraser: this is used to indicate that the *target* specified (next section) must be removed or reset. Some of those targets are:
  - response node: there is a twisted use of the response body as a temporary test-bed template. It consists in inserting auxiliary nodes to be used as valid sources within provision transformations, and remove them before sending the response. Note that nonexistent nodes become null nodes when removed, so take care if you don't want this. When the eraser applies to response node root, it just removes response body.
  - global variable: the user should remove this kind of variables after last flow usage to avoid memory growth in load testing. Global variables are not confined to an specific provision context (where purge procedure is restricted to the event history server data), so the eraser is the way to proceed when it comes to free the global list and reduce memory consumption.
  - event: we could purge storage events, something that could be necessary to control memory growth in load testing.
  - with other kind of targets, eraser acts like setting an empty string.

- math.`<expression>`: this source is based in [Arash Partow's exprtk](https://github.com/ArashPartow/exprtk) math library compilation. There are many possibilities (calculus, control and logical expressions, trigonometry, logic, string processing, etc.), so check [here](https://github.com/ArashPartow/exprtk/blob/master/readme.txt) for more information. This source specification **admits variables substitution** (third-party library variable substitutions are not needed, so they are not supported). Some simple examples could be: "2*sqrt(2)", "sin(3.141592/2)", "max(16,25)", "1 and 1", etc. You may implement a simple arithmetic server (check [this](./kata/09.Arithmetic_Server/README.md) kata exercise to deepen the topic).

- random.`<min>.<max>`: integer number in range `[min, max]`. Negatives allowed, i.e.: `"-3.+4"`.

- randomset.`<value1>|..|<valueN>`: random string value between pipe-separated labels provided. This source specification **admits variables substitution**. Note that both leading and trailing pipes would add empty parts (`'|foo|bar'`, `'foo|bar|'` and `'foo||bar'` become three parts, `'foo'`, `'bar'` and empty string).

- timestamp.`<unit>`: UNIX epoch time in `s` (seconds), `ms` (milliseconds), `us` (microseconds) or `ns` (nanoseconds).

- strftime.`<format>`: current date/time formatted by [strftime](https://www.cplusplus.com/reference/ctime/strftime/). This source format **admits variables substitution**.

- recvseq: sequence id number increased for every mock reception (starts on *1* when the *h2agent* is started).

- var.`<id>`: general purpose variable (readable at transformation chain, scoped to the `outState` chain). Cannot refer json objects. This source variable identifier **admits variables substitution**. Variables set in one provision are automatically available in the next provision linked via `outState`, and destroyed when the chain ends.

- globalVar.`<id>`: general purpose global variable (readable from anywhere, process-level scope). Cannot refer json objects. This source variable identifier **admits variables substitution**. Global variables are useful to store dynamic information to be used in a different provision instance. For example you could split a request `URI` in the form `/update/<id>/<timestamp>` and store a variable with the name `<id>` and value `<timestamp>`. That variable could be queried later just providing `<id>` which is probably enough in such context. Thus, we could parse other provisions (access to events addressed with dynamic elements), simulate advanced behaviors, or just parse mock invariant globals over configured provisions (although this seems to be less efficient than hard-coding them, it is true that it drives provisions adaptation "on the fly" if you update such globals when needed).

- value.`<value>`: free string value. Even convertible types are allowed, for example: integer string, unsigned integer string, float number string, boolean string (true if non-empty string), will be converted to the target type. Empty value is allowed, for example, to set an empty string, just type: `"value."`. This source value **admits variables substitution**. Also, special characters are allowed ('\n', '\t', etc.).

- serverEvent.`<server event address in query parameters format>`: access server context indexed by request *method* (`requestMethod`), *URI* (`requestUri`), events *number* (`eventNumber`) and events number *path* (`eventPath`), where query parameters are:

  - *requestMethod*: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*). Mandatory.
  - *requestUri*: event *URI* selected. Mandatory.
  - *eventNumber*: position selected (*1..N*; *-1 for last*) within events list. Mandatory.
  - *eventPath*: `json` document path within selection. Optional.

  Event addressing will retrieve a `json` object corresponding to a single event (given by `requestMethod`, `requestUri` and `eventNumber`) and optionally a node within that event object (given by `eventPath` to narrow the selection).

  For example, `serverEvent.requestMethod=GET&requestUri=/foo/var&eventNumber=3&eventPath=/requestHeaders` searches the third (event number 3) `GET /foo/bar` request and `/requestHeaders` path, as part of event definition, gives the request headers that was received. The particular case of empty event path extracts the whole event structure, and in general, paths are [json pointers](https://tools.ietf.org/html/rfc6901), which are powerful enough to cover addressing needs.

  **Important note**: as this source provides a list of query parameters, and one of these parameters is a *URI* itself (`requestUri`) it is important to know that it may need to be URL-encoded to avoid ambiguity with query parameters separators ('=', '&'). So for example, in case that request *URI* contains other query parameters, you must encode it within the source definition. Consider this one: `/app/v1/stock/madrid?loc=123&id=2`. You could use `./tools/url.sh` script helper to prepare its encoded version:

  ```bash
  $ tools/url.sh --encode "/app/v1/stock/madrid?loc=123&id=2"

  Encoded URL:/app/v1/stock/madrid%3Floc%3D123%26id%3D2
  ```

  So, for this example, a source could be the following:

  `serverEvent.requestMethod=POST&requestUri=/app/v1/stock/madrid%3Floc%3D123%26id%3D2&eventNumber=-1&eventPath=/requestBody`

  Once tokenized, each query parameter is decoded just in case it is needed, and that request *URI* becomes the one desired.

  But there is a more intuitive way to proceed to solve this, because as this source value **admits variables substitution**, we could assign query parameters as variables in previous transformations, and then assign the following generic source: `serverEvent.requestMethod=@{requestMethod}&requestUri=@{requestUri}&eventNumber=@{eventNumber}&eventPath=@{eventPath}`

  This way, user <u>does not have to be worried about encoding,</u> because query parameters are correctly interpreted ('@' and curly braces are not an issue for URL encoding) and replaced during source processing, so for example we could use that generic source definition or something more specific for request *URI* which is the problematic one:

  `serverEvent.requestMethod=POST&requestUri=@{requestUri}&eventNumber=-1&eventPath=/requestBody`

  where `requestUri` would be a variable defined before with the value directly decoded: `/app/v1/stock/madrid?loc=123&id=2`.

  <u>Only in the case that request *URI* is simple enough</u> and does not break the whole server event query parameter list definition, we could just define this source in one line without need to encode or use auxiliary variables, being the most simplified and smart way to define event sources.

  <u>Server events history</u> should be kept enabled allowing to access events. So, imagine the following current server data map:

  ```json
  [
    {
      "method": "POST",
      "events": [
        {
          "requestBody": {
            "engine": "tdi",
            "model": "audi",
            "year": 2021
          },
          "requestHeaders": {
            "accept": "*/*",
            "content-length": "52",
            "content-type": "application/x-www-form-urlencoded",
            "user-agent": "curl/7.77.0"
          },
          "previousState": "initial",
          "receptionTimestampUs": 1626039610709978,
          "responseDelayMs": 0,
          "responseStatusCode": 201,
          "serverSequence": 116,
          "state": "initial"
        }
      ],
      "uri": "/app/v1/stock/madrid?loc=123&id=2"
    }
  ]
  ```

  Then, the source commented above would store this `json` object, which is the request body for the last (`eventNumber=-1`) event registered:

  ```json
  {
    "engine": "tdi",
    "model": "audi",
    "year": 2021
  }
  ```

- inState: current processing state.

- clientEvent.`<client event address in query parameters format>`: access client context indexed by client endpoint identifier (`clientEndpointId`), request *method* (`requestMethod`), *URI* (`requestUri`), events *number* (`eventNumber`) and events number *path* (`eventPath`), where query parameters are:

  - *clientEndpointId*: client endpoint identifier. Mandatory.
  - *requestMethod*: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*). Mandatory.
  - *requestUri*: event *URI* selected. Mandatory.
  - *eventNumber*: position selected (*1..N*; *-1 for last*) within events list. Mandatory.
  - *eventPath*: `json` document path within selection. Optional.

  Event addressing will retrieve a `json` object corresponding to a single client event (given by `clientEndpointId`, `requestMethod`, `requestUri` and `eventNumber`) and optionally a node within that event object (given by `eventPath` to narrow the selection).

  For example, `clientEvent.clientEndpointId=myBackend&requestMethod=GET&requestUri=/api/v1/data&eventNumber=-1&eventPath=/responseBody` searches the last client event for `GET /api/v1/data` sent through the `myBackend` endpoint, and `/responseBody` path gives the response body that was received.

  This source **admits variables substitution** and follows the same URL-encoding considerations as `serverEvent` for the `requestUri` parameter.

  <u>Client events history</u> should be kept enabled allowing to access events. This source is available in both server and client provision transformations, enabling server provisions to read data from previous client interactions (e.g., to build responses based on data fetched from a backend).

- txtFile.`<path>`: reads text content from file with the path provided. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. Note that paths to missing files will fail to open. This source enables the `h2agent` capability to serve files.

- binFile.`<path>`: same as `txtFile` but reading binary data.

- command.`<command>`: executes command on process shell and captures the standard output/error ([popen](https://man7.org/linux/man-pages/man3/popen.3.html)() is used behind). Also, the return code is saved into scoped variable `rc`. You may call external scripts or executables, and do whatever needed as if you would be using the shell environment.

  - Important notes:
    - **Be aware about security problems**, as you could provision via `REST API` any instruction accessible by a running `h2agent` to extract information or break things without interface restriction (remember anyway that `h2agent` supports [secured connection](#Execution-with-TLS-support)).
    - **This operation could impact performance** as external procedures will block the working thread during execution (it is different than response delays which are managed asynchronously), so perhaps you should increase the number of working threads (check [command line](#Command-line)). This operation is mainly designed to run administrative procedures within the testing flow, but not as part of regular provisions to define mock behavior. So, having an additional working thread (`--traffic-server-worker-threads 2`) should be enough to handle dedicated `URIs` for that kind of work reserving another thread for normal traffic.

  - Examples:
    - `/any/procedure 2>&1`: `stderr` is also captured together with standard output (if not, the `h2agent` process will show the error message in console).
    - `ls /the/file 2>/dev/null || /bin/true`: always success (`rc` stores 0) even if file is missing. Path captured when the file path exists.
    - `/opt/tools/checkCondition &>/dev/null && echo fulfilled`: prepare transformation to capture non-empty content ("fulfilled") when condition is successful.
    - `/path/to/getJpg >/var/log/image.jpg 2>/var/log/getJpg.err`: arbitrary procedure executed and standard output/error dumped into files which can be read in later step by mean `binFile`/`txtFile` sources.
    - Shell commands accessible on environment path: security considerations are important but this functionality is worth it as it even allows us to simulate exceptional conditions within our test system. For example, we could provision a special `uri` to provoke the mock server crash using command source: `pkill -SIGSEGV h2agent` (suicide command).




The **target** of information is classified after parsing the following possible expressions (between *[square brackets]* we denote the potential data types allowed):

- response.body.string *[string]*: response body storing expected string processed.

- response.body.hexstring *[string]*: response body storing expected string processed from hexadecimal representation, for example `0x8001` (prefix `0x` is optional).

- response.body.json.string *[string]*: response body document storing expected string at *root*.

- response.body.json.integer *[integer]*: response body document storing expected integer at *root*.

- response.body.json.unsigned *[unsigned integer]*: response body document storing expected unsigned integer at *root*.

- response.body.json.float *[float number]*: response body document storing expected float number at *root*.

- response.body.json.boolean *[boolean]*: response body document storing expected boolean at *root*.

- response.body.json.object *[json object]*: response body document storing expected object as *root* node.

- response.body.json.jsonstring *[json string]*: response body document storing expected object, extracted from json-parsed string, as *root* node.

- response.body.json.string.`/<node1>/../<nodeN>` *[string]*: response body node path storing expected string. This target path **admits variables substitution**.

- response.body.json.integer.`/<node1>/../<nodeN>` *[integer]*: response body node path storing expected integer. This target path **admits variables substitution**.

- response.body.json.unsigned.`/<node1>/../<nodeN>` *[unsigned integer]*: response body node path storing expected unsigned integer. This target path **admits variables substitution**.

- response.body.json.float.`/<node1>/../<nodeN>` *[float number]*: response body node path storing expected float number. This target path **admits variables substitution**.

- response.body.json.boolean.`/<node1>/../<nodeN>` *[boolean]*: response body node path storing expected booblean. This target path **admits variables substitution**.

- response.body.json.object.`/<node1>/../<nodeN>` *[json object]*: response body node path storing expected object under provided path. If source origin is not an object, there will be a best effort to convert to string, number, unsigned number, float number and boolean, in this specific priority order. This target path **admits variables substitution**.

- response.body.json.jsonstring.`/<node1>/../<nodeN>` *[json string]*: response body node path storing expected object, extracted from json-parsed string, under provided path. This target path **admits variables substitution**.

- response.header.`<hname>` *[string (or number as string)]*: response header component (i.e. *location*). This target name **admits variables substitution**. Take into account that header fields values are sent [lower cased](https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2).

- response.statusCode *[unsigned integer]*: response status code.

- response.delayMs *[unsigned integer]*: simulated delay to respond: although you can configure a fixed value for this property on provision document, this transformation target overrides it.

- var.`<id>` *[string (or number as string)]*: general purpose variable (writable at transformation chain, scoped to the `outState` chain). The idea of *variable* vaults is to optimize transformations when multiple transfers are going to be done (for example, complex operations like regular expression filters, are dumped to a variable, and then, we drop its value over many targets without having to repeat those complex algorithms again). Variables set here are automatically available in subsequent provisions linked via `outState`. Cannot store json objects. This target variable identifier **admits variables substitution**.

- globalVar.`<id>` *[string (or number as string)]*: general purpose global variable (writable at transformation chain and intended to be used later, as source, from anywhere as process-level scope). Cannot refer json objects. This target variable identifier **admits variables substitution**. <u>Target value is appended to the current existing value</u>. This allows to use global variables as memory buckets. So, you must use `eraser` to reset its value guaranteeing it starts from scratch.

- outState *[string (or number as string)]*: next processing state. This overrides the default provisioned one.

- outState.`[POST|GET|PUT|DELETE|HEAD][.<uri>]` *[string (or number as string)]*: next processing state for specific method (virtual server data will be created if needed: this way we could modify the flow for other methods different than the one which is managing the current provision). This target **admits variables substitution** in the `uri` part.

  You could, for example, simulate a database where a *DELETE* for an specific entry could infer through its provision an *out-state* for a foreign method like *GET*, so when getting that *URI* you could obtain a *404* (assumed this provision for the new *working-state* = *in-state* = *out-state* = "id-deleted"). By default, the same `uri` is used from the current event to the foreign method, but it could also be provided optionally giving more flexibility to generate virtual events with specific states.

- txtFile.`<path>` *[string]*: dumps source (as string) over text file with the path provided. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. Note that paths to missing directories will fail to open (the process does not create tree hierarchy). It is considered long term file (file is closed 1 second after last write, by default) when a constant path is configured, because this is normally used for specific log files. On the other hand, when any substitution may took place in the path provided (it has variables in the form `@{varname}`) it is considered as a dynamic name, so understood as short term file (file is opened, written and closed without delay, by default). **Note:** you can force short term type inserting a variable, for example with empty value: `txtFile./path/to/short-term-file.txt@{empty}`. Delays in microseconds are configurable on process startup. Check  [command line](#Command-line) for `--long-term-files-close-delay-usecs` and `--short-term-files-close-delay-usecs` options.

  This target can also be used to write named pipes (previously created: `mkfifo /tmp/mypipe && chmod 0666 /tmp/mypipe`), with the following restriction: writes must close the file descriptor everytime, so long/short term delays for close operations must be zero depending on which of them applies: variable paths zeroes the delay by default, but constant ones shall be zeroed too by command-line (`--long-term-files-close-delay-usecs 0`). Just like with regular UNIX pipes (`|`), when the writer closes, the pipe is torn down, so fast operations writting named pipes could provoke data looses (some writes missed). In that case, it is more recommended to use UDP through unix socket target (`udpSocket./tmp/udp.sock`).

- binFile.`<path>` *[string]*: same as `txtFile` but writting binary data.

- udpSocket.`<path>[|<milliseconds delay>]` *[string]*: sends source (as string) via UDP datagram through a unix socket with the path provided, with an optional delay in milliseconds. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. UDP is a transport layer protocol in the TCP/IP suite, which provides a simple, connectionless, and unreliable communication service. It is a lightweight protocol that does not guarantee the delivery or order of data packets. Instead, it allows applications to send individual datagrams (data packets) to other hosts over the network without establishing a connection first. UDP is often used where low latency is crucial. In `h2agent` is useful to signal external applications to do associated tasks sharing specific data for the transactions processed. Use `./tools/udp-server` program to play with it or even better `./tools/udp-server-h2client` to generate HTTP/2 requests UDP-driven (this will be covered when full `h2agent` client capabilities are ready).

- serverEvent.`<server event address in query parameters format>`: this target is always used in conjunction with `eraser` source acting as an alternative purge method to the purge `outState`. The main difference is that states-driven purge method acts over processed events key (`method` and `uri` for the provision in which the purge state is planned), so not all the test scenarios may be covered with that constraint if they need to remove events registered for different transactions. In this case, event addressing is defined by request *method* (`requestMethod`), *URI* (`requestUri`), and events *number* (`eventNumber`): events number *path* (`eventPath`) is not accepted, as this operation just remove specific events or whole history, like REST API for server-data deletion:

  - *requestMethod*: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*). Mandatory.
  - *requestUri*: event *URI* selected. Mandatory.
  - *eventNumber*: position selected (*1..N*; *-1 for last*) within events list. Optional: if not provided, all the history may be purged.

  This target, as its source counterpart, **admits variables substitution**.

- clientProvision.`<clientProvisionId>`.`<inState>` *[string]*: triggers a client provision flow (fire-and-forget) from within a server transformation. Both the identifier and the `inState` **admit variables substitution** (e.g. `clientProvision.@{flowId}.@{myState}`). Multiple `clientProvision` targets can be specified in the same transformation list and all of them will be triggered. Triggers are collected during the transformation pipeline and executed asynchronously after the server response is fully built, so they do not block or delay the server response. This is the mechanism to connect server and client modes: when the server receives a request, it can trigger one or more outgoing client flows as a side effect.

  The source acts as a **conditional gate**: the trigger only fires when the source resolves to a non-empty value. An empty or undefined source (e.g. a variable that does not exist, or `eraser`) silently skips the trigger. This follows the same convention as the `break` target.

  ```json
  {
    "source": "value.1",
    "target": "clientProvision.myNotificationFlow.initial"
  }
  ```

  Conditional triggering based on a flag variable (no `ConditionVar` filter needed):

  ```json
  {
    "source": "var.triggerFlow",
    "target": "clientProvision.myNotificationFlow.initial"
  }
  ```

  Here, if `var.triggerFlow` exists and is non-empty, the trigger fires. If the variable does not exist (resolves to empty), the trigger is skipped.

  Using `eraser` as source deliberately disables the trigger while keeping the provision configured — useful for temporarily deactivating a flow without removing the transformation:

  ```json
  {
    "source": "eraser",
    "target": "clientProvision.myNotificationFlow.initial"
  }
  ```

  The triggered client provision must be previously configured (via `POST /admin/v1/client-provision`) along with its associated client endpoint (via `POST /admin/v1/client-endpoint`). If the provision or endpoint is not found, or the endpoint is disabled, an error is logged and the trigger is silently skipped.

- break *[string]*: when non-empty string is transferred, the transformations list is interrupted. Empty string (or undefined source) ignores the action. Note that placing `break` as the **last** item in a transformation list is illogical — there are no further items to interrupt. A warning is logged at provision time when this is detected.



There are several **filter** methods, but remember that filter node is optional, so you could directly transfer source to target without modification, just omitting filter, for example:

```json
{
  "source": "random.25.35",
  "target": "response.delayMs"
}
```

In the case above, *delay* will take the absolute value for the random generated (just in case the user configures a range with possible negative result).

Filters give you the chance to make complex transformations:



- RegexCapture: this filter provides a regular expression, including optionally capture groups which will be applied to the source and stored in the target. This filter is designed specially for general purpose variables, because each captured group *k* will be mapped to a new variable named `<id>.k` where `<id>` is the original source variable name. Also, the variable "as is" will store the entire match, same for any other type of target (used together with boolean target it is useful to write the match condition). Let's see some examples:

  ```json
  {
    "source": "request.uri.path",
    "target": "var.id_cat",
    "filter": { "RegexCapture" : "\/api\/v2\/id-([0-9]+)\/category-([a-z]+)" }
  }
  ```

  In this case, if the source received is *"/api/v2/id-28/category-animal"*, then we have 2 captured groups, so, we will have: *var.id_cat.1="28"* and *var.id_cat.2="animal"*. Also, the specified variable name *"as is"* will store the entire match: *var.id_cat="/api/v2/id-28/category-animal"*.

  Other example:

  ```json
  {
    "source": "request.uri.path",
    "target": "response.body.json.string./category",
    "filter": { "RegexCapture" : "\/api\/v2\/id-[0-9]+\/category-([a-z]+)" }
  }
  ```

  In this example, it is not important to notice that we only have 1 captured group (we removed the brackets of the first one from the previous example). This is because the target is a path within the response body, not a variable, so, only the entire match (if proceed) will be transferred. Assuming we receive the same source from previous example, that value will be the entire *URI* path. If we would use a variable as target, such variable would store the same entire match, and also we would have *animal* as `<variable name>.1`.

  If you want to move directly the captured group (`animal`) to a non-variable target, you may use the next filter:



- RegexReplace: this is similar to the matching algorithm based in regular expressions and replace procedure (even the fact that it *falls back to source information when not matching is done*, something that differs from former `RegexCapture` algorithm which builds an empty string when regular expression is not fully matched). We provide `rgx` and `fmt` to transform the source into the target:

  ```json
  {
    "source": "request.uri.path",
    "target": "response.body.json.unsigned./data/timestamp",
    "filter": {
      "RegexReplace" : {
        "rgx" : "(/ctrl/v2/id-[0-9]+/)ts-([0-9]+)",
        "fmt" : "$2"
      }
    }
  }
  ```

  For example, if the source received is "*/ctrl/v2/id-555112233/ts-1615562841*", then we will replace/create a node "*data.timestamp*" within the response body, with the value formatted: *1615562841*.

  In this algorithm, the obtained value will be a string.

  Another useful example could be the transformation from a sequence (*msisdn*, phone number, etc.) into an idempotent associated *IPv4*. A simple algorithm could consists in getting the 8 less significant digits (as they are also valid hexadecimal signs) and build the *IPv4* representation in this way:

  ```json
  {
    "source": "request.body.phone",
    "target": "var.ipv4",
    "filter": {
      "RegexReplace" : {
        "rgx" : "[0-9]+([0-9]{2})([0-9]{2})([0-9]{2})([0-9]{2})",
        "fmt" : "$1.$2.$3.$4"
      }
    }
  }
  ```

  Although an specific filter could be created ad-hoc for *IPv4* (or even *IPv6* or whatever), we don't consider at the moment that the probably better performance of such implementations, justify abandon the flexibility in the current abstraction that we achieve thanks to the *RegexReplace* filter.



- Append: this appends the provided information to the source. This filter, **admits variables substitution**.

  ```json
  {
    "source": "value.telegram",
    "target": "var.site",
    "filter": { "Append" : ".teslayout.com" }
  }
  ```

  In the example above we will have *var.site="telegram.teslayout.com"*.

  This could be done also with the `RegexReplace` filter, but this has better performance.

  In this algorithm, the obtained value will be a string.

  The advantage against "value-type source with variables replace", is that we can operate directly any source type without need to store auxiliary variable to be replaced.



- Prepend: this prepends the provided information to the source. This filter, **admits variables substitution**.

  ```json
  {
    "source": "value.teslayout.com",
    "target": "var.site",
    "filter": { "Prepend" : "www." }
  }
  ```

  In the example above we will have *var.site="telegram.teslayout.com"*.

  This could be done also with the `RegexReplace` filter, but this has better performance.

  In this algorithm, the obtained value will be a string.

  The advantage against "value-type source with variables replace", is that we can operate directly any source type without need to store auxiliary variable to be replaced.



- Sum: adds the source (if numeric conversion is possible) to the value provided (which <u>also could be negative or float</u>):

  ```json
  {
    "source": "random.0.99999999",
    "target": "var.mysum",
    "filter": { "Sum" : 123456789012345 }
  }
  ```

  In this example, the random range limitation (integer numbers) is uncaged through the addition operation. Using this together with other filter algorithms should complete most of the needs. For more complex operations, you may use the `math` source.

  This filter is also useful to sequence a subscriber number:

  ```json
  {
    "source": "recvseq",
    "target": "var.subscriber",
    "filter": { "Sum" : 555000000 }
  }
  ```

  It is not valid to provide algebraic expressions (like 1/3, 2^5, etc.). For more complex operations, you may use the `math` source.



- Multiply: multiplies the source (if numeric conversion is possible) by the value provided (which <u>also could be negative to change sign, or lesser than 1 to divide</u>):

  ```json
  {
    "source": "value.-10",
    "target": "var.value-of-one",
    "filter": { "Multiply" : -0.1 }
  }
  ```

  In this example, we operate `-10 * -0.1 = 1`. It is not valid to provide algebraic expressions (like 1/3, 2^5, etc.). For more complex operations, you may use the `math` source.



- ConditionVar: conditional transfer from source to target based on the boolean interpretation of the string-value stored in the variable (both local and global variables are searched, giving <u>priority to local ones</u>), which is:

  - **False** condition for cases:
    - <u>Undefined</u> variable.
    - Defined but <u>empty</u> string.

  - **True** condition for the rest of cases:
    - Defined variable with <u>non-empty</u> value: note that "0", "false" or any other "apparently false" non-empty string could be misinterpreted: they are absolutely true condition variables.
      Also, variable name in `ConditionVar` filter, can be preceded by <u>exclamation mark (!)</u>  in order to <u>invert the condition</u>.

  Transfer procedure consists in <u>source copy over target</u> only when condition is **true**. To assign another value for **false** condition, you must use the 	inverted variable in another transformation item (no ternary syntax collapsed in single item is available):

  ```json
  {
    "source": "value.value when id is true",
    "target": "response.body.string",
    "filter": { "ConditionVar" : "id" }
  },
  {
    "source": "value.value when id is false",
    "target": "response.body.string",
    "filter": { "ConditionVar" : "!id" }
  }
  ```

  Normally, we generate condition variables by mean regular expression filters, because non-matched sources skips target assignment (undefined is *false* condition) and matched ones copy the source (matched) into the target (variable) which will be a compliant condition variable (non-empty string is *true* condition):

  ```json
  {
    "source": "request.body./must/be/number",
    "target": "var.isNumber",
    "filter": { "RegexCapture" : "([0-9]+)" }
  }
  ```

  In that example `isNumber` will be undefined (**false** as condition variable) if the request body node value at `/must/be/number` is not a number, and will hold that numeric value, so non-empty value (**true** as condition variable), when it is actually a number (guaranteed by regular expression filter). Then, we can use it as condition variable:

  ```json
  {
    "source": "value.number received !",
    "target": "response.body.string",
    "filter": { "ConditionVar" : "isNumber" }
  }
  ```

  Condition variables may also be created **automatically** by some transformations into variable targets, to be used later in this `ConditionVar` filter. The best example are `JsonConstraint` and `SchemaId` filters (explained later) working together with variable target, as it outputs "1" when validation is successful and "" when fails.

  There are some other transformations that are mainly used to create condition variables to be used later. This is the case of *EqualTo* and *DifferenFrom*:



- EqualTo: conditional transfer from source to target based in string comparison between the source and the provided value. This filter, **admits variables substitution**.

  ```json
  {
    "source": "request.body",
    "target": "var.expectedBody",
    "filter": { "EqualTo" : "{\"foo\":1}" }
  },
  {
    "source": "value.400",
    "target": "response.statusCode",
    "filter": { "ConditionVar" : "!expectedBody" }
  }
  ```

  We could also <u>insert the whole condition in the source</u> using for example math library functions `like` and `ilike` (case insensitive variant), having a normalized output ("0": false, "1": true) to compare with filter value:

  ```json
  {
    "source": "math.'@{name1}' ilike 'word'",
    "target": "var.iequal",
    "filter": { "EqualTo" : "1" }
  }
  ```

  Math library also supports wild-cards for string comparisons and many advanced operations, but normally `RegexCapture` is a better alternative (for example: "`[w|W][o|O][r|R][d|D]`" matches "word" as well as "wOrD" or any other combination) because it is more efficient: math library is always used with dynamic variables, so it needs to be compiled on-the-fly, but regular expressions used in `h2agent` are always compiled at provision stage.

  Perhaps, the only use cases that require math library are those related to numeric comparisons:

  In the following example, we translate a logical math expression (which results in value of `1` (true) or `0` (false)) into conditional variable, because it will hold the value "1" or nothing (remember: conditional transfer):

  ```json
  {
    "source": "recvseq",
    "target": "var.recvseq"
  },
  {
    "source": "math.@{recvseq} > 10",
    "target": "var.greater",
    "filter": { "EqualTo" : "1" }
  },
  {
    "source": "value.Sequence @{recvseq} is lesser or equal than 10",
    "target": "response.body.string"
  },
  {
    "source": "value.Sequence @{recvseq} is greater than 10",
    "target": "response.body.string",
    "filter": { "ConditionVar" : "greater" }
  }
  ```

  We could also generate conditional variables from logical expressions using math library and `EqualTo` filter to normalize the result into a compliant conditional variable:

  ```json
  {
    "source": "math.@{A}*@{B}",
    "filter": { "EqualTo" : "1" },
    "target": "var.A_and_B"
  },
  {
    "source": "math.max(@{A},@{B})",
    "filter": { "EqualTo" : "1" },
    "target": "var.A_or_B"
  },
  {
    "source": "math.abs(@{A}-@{B})",
    "filter": { "EqualTo" : "1" },
    "target": "var.A_xor_B"
  }
  ```

  Note that `A_xor_B` could be also obtained using source `(@{A}-@{B})^2` or `(@{A}+@{B})%2`.



- DifferentFrom: conditional transfer from source to target based in string comparison between the source and the provided value. This filter, **admits variables substitution**. Its use is similar to `EqualTo` and complement its logic in case we need to generate the negated variable.



- JsonConstraint: performs a `json` validation between the source (must be a valid document) and the provided filter `json` object.

  - If validation **succeed**, the string "1" is stored in selected target.
  - If validation **fails**, the validation report detail is stored in selected target. <u>If the target is a variable</u> (recommended use), the validation report is stored in `<varname>.fail` variable, and `<varname>` will be emptied. So we could use `!<varname>` or `<varname>.fail` as equivalent condition variables to detect the validation error.

  ```json
  {
    "source": "request.body",
    "target": "var.expectedBody",
    "filter": { "JsonConstraint" : {"foo":1} }
  },
  {
    "source": "value.400",
    "target": "response.statusCode",
    "filter": { "ConditionVar" : "!expectedBody" }
  },
  {
    "source": "var.expectedBody.fail",
    "target": "response.body.string",
    "filter": {
      "ConditionVar": "expectedBody.fail"
    }
  },
  {
    "source": "var.expectedBody.fail",
    "target": "break"
  }
  ```

  Validation algorithm consists in object reference restriction over source (which must be an object). So, everything included in the filter must exist and be equal to source, but could miss information (for which it would be non-restrictive). So, an empty object '{}' always matches (although it has no sense to be used). In the example above, `{"foo":1}` is validated, but also `{"foo":1,"bar":2}` does.

  To understand better, imagine the source as the 'received' body, and the json constraint filter object as the 'expected' one, so the restriction is ruled by 'expected' acting as a subset which could miss/ignore nodes actually received without problem (less restrictive), but those ones specified there, must exist and be equal to the ones received.

  Take into account that filter provides an static object where variables search/replace is not possible, so those elements which could be non-trivial should be validated separately, for example:

  ```json
  {
    "source":"request.body./here/the/id",
    "filter": { "EqualTo": "@{id}" },
    "target": "var.idMatches"
  }
  ```

  And finally, we should aggregate condition results related to the event analyzed, to compute a global validation result.

  The amount of transformation items is approximately the same as if we could adapt the json constraint (as we would need items to transfer dynamic data like `id` in the example, to the corresponding object node), indeed it seems more intuitive to use `JsonConstraint` for static references:

  Many times, dynamic values are node keys instead of values, so we could still use `JsonConstraint` if nested information is static/predictable.

  ```json
  {
    "source": "request.body./data/@{phone}",
    "target": "var.expectedPhoneNodeWithinBody",
    "filter": {
      "JsonConstraint": {
        "model": "samsung",
        "color": "blue"
      }
    }
  }
  ```

  Often, most of the needed validation documents will be known *a priori* within certain testing conditions, so dynamic validations by mean other filters should be minimized.

  Multiple validations in different tree locations with different filter objects could be chained. Imagine that we received this one:

  ```json
  {
    "foo": 1,
    "timestamp": 1680710820,
    "data": {
      "555555555": {
        "model": "samsung",
        "color": "blue"
      }
    }
  }
  ```

  Then, these could be the whole validation logic in our provision:

  ```json
  {
    "source": "request.body",
    "target": "var.rootDataOK",
    "filter": {
      "JsonConstraint": {
        "foo": 1
      }
    }
  },
  {
    "source": "request.uri.param.phone",
    "target": "var.phone"
  },
  {
    "source": "request.body./data/@{phone}",
    "target": "var.phoneDataOK",
    "filter": {
      "JsonConstraint": {
        "model": "samsung",
        "color": "blue"
      }
    }
  },
  {
    "source": "value.@{rootDataOK}@{phoneDataOK}",
    "filter": {
      "EqualTo": "11"
    },
    "target": "var.allOK"
  }
  ```

  Where the time-stamp received from the client is omitted as unpredictable in the first validation, and the phone (`555555555`), supposed (in the example) to be provided in the request query parameters list, is validated through its nested content against the corresponding request node path (`/data/555555555`).

  To finish, just to remark that a mock server used for functional tests can also be inspected through *REST API*, retrieving any event related data to be externally validated, so we will not need to make complicated provisions to do that internally, or at least we could make a compromise between internal and external validations. The difference is the fact that self-contained provisions could "make the day" against scattered information between those provisions and test orchestrator. Also remember that schema validation is supported, so you could provide an OpenAPI restriction for your project interfaces.

  Provisions identification through method and *URI* is normally enough to decide rejecting with 501 (not implemented), although this can be enforced with `JsonConstraint` filter in order to be more accurate if needed. In the case of load testing, normally we are not so strict in favor of performance regarding flow validations. Definitely, this filter is mainly used to validate responses in client mock mode.



- SchemaId: performs a `json` schema validation between the source (must be a valid document) and the provided filter which is a registered schema for the given identifier. Same logic than `JsonConstraint` is applied here:

  - If validation **succeed**, the string "1" is stored in selected target.

  - If validation **fails**, the validation report detail is stored in selected target. <u>If the target is a variable</u> (recommended use), the validation report is stored in `<varname>.fail` variable, and `<varname>` will be emptied. So we could use `!<varname>` or `<varname>.fail` as equivalent condition variables to detect the validation error.



  Both the `JsonConstraint` and `SchemaId` filters **serve as more specific supplementary validations** to enhance event schemas (request and response validation schemas).



Finally, after possible transformations, we could validate the response body (although this may be considered overkilling because the mock is expected to build the response according with a known response schema):



### Multiple provisions

Provision of a set of provisions through an array object is allowed. So, instead of launching *N* provisions separately, you could group them:

```json
[
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/1",
    "responseCode": 200,
    "responseBody": { "foo": "bar-1" },
    "responseHeaders": { "content-type": "application/json", "x-version": "1.0.0" }
  },
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/2",
    "responseCode": 200,
    "responseBody": { "foo": "bar-2" },
    "responseHeaders": { "content-type": "application/json", "x-version": "1.0.0" }
  }
]
```

A provision set fails with the first failed item, giving a 'pluralized' version of the single provision failed response message although previous valid provisions will be added.

### Unused provisions

`GET /admin/v1/server-provision/unused` retrieves all the provisions configured that were not used yet. This is useful for troubleshooting (during tests implementation or *SUT* updates) to filter unnecessary provisions configured: when the test is executed, just identify unused items and then remove them from test configuration.
The 'unused' status is initialized at creation time (`POST` operation) or when the provision is overwritten.

## Server data

### Storage configuration

Both server and client data storage share the same configuration model (`PUT /admin/v1/server-data/configuration`).

There are three valid combinations:

* `discard=true&discardKeyHistory=true`: nothing is stored.
* `discard=false&discardKeyHistory=true`: no key history stored (only the last event for a key, except for unprovisioned events, which history is always respected for troubleshooting purposes).
* `discard=false&discardKeyHistory=false`: everything is stored: events and key history.

The combination `discard=true&discardKeyHistory=false` is incoherent, as it is not possible to store requests history with general events discarded. In this case, an status code *400 (Bad Request)* is returned.

The `h2agent` starts with full data storage enabled by default, but you could also disable this through command-line (`--discard-data` / `--discard-data-key-history`).

And regardless the previous combinations, you could enable or disable the purge execution when this reserved state is reached for a specific provision:

* `disablePurge=true`: provisions with `purge` state will ignore post-removal operation when this state is reached.
* `disablePurge=false`: provisions with `purge` state will process post-removal operation when this state is reached.

The `h2agent` starts with purge stage enabled by default, but you could also disable this through command-line (`--disable-purge`).

Be careful using this `PUT` operation in the middle of traffic load, because it could interfere and make unpredictable the server data information during tests. Indeed, some provisions with transformations based in event sources, could identify the requests within the history for an specific event assuming that a particular server data configuration is guaranteed.

`GET /admin/v1/server-data/configuration` retrieves the current configuration:

```json
{
    "needsStorage": false,
    "purgeExecution": true,
    "storeEvents": true,
    "storeEventsKeyHistory": true
}
```

The `needsStorage` field is a computed boolean: `true` when any loaded provision contains transformation items with source type `serverEvent`/`clientEvent` or target type `serverEventToPurge`/`clientEventToPurge`. This allows runners to auto-detect whether storage must be enabled for the current test scenario.

#### Stateful detection warnings

When a provision is loaded (`POST`) that references event-dependent transformations but `storeEvents` is currently disabled, the response includes a `warning` field:

```json
{
    "result": "true",
    "response": "server-provision operation; valid schema and server provision data received",
    "warning": "provision references serverEvent/clientEvent but storeEvents is disabled"
}
```

Similarly, when storage is disabled via `PUT /admin/v1/server-data/configuration?discard=true` but loaded provisions require it, the response includes a warning body:

```json
{
    "result": "true",
    "response": "server-data configuration updated",
    "warning": "active provisions require storeEvents (serverEvent/clientEvent references found)"
}
```

Both are advisory only — the operation proceeds normally.

By default, the `h2agent` enables both kinds of storage types (general events and requests history events), and also enables the purge execution if any provision with this state is reached, so the previous response body will be returned on this query operation. This is useful for function/component testing where more information available is good to fulfill the validation requirements. In load testing, we could seize the `purge` out-state to control the memory consumption, or even disable storage flags in case that test plan is stateless and allows to do that simplification.

### Querying server data

`GET /admin/v1/server-data` retrieves the current server internal data (requests received, their states and other useful information like timing or global order). Events received are stored <u>even if no provisions were found</u> for them (the agent answered with `501`, not implemented), being useful to troubleshoot possible configuration mistakes in the tests design. By default, the `h2agent` stores the whole history of events (for example requests received for the same `method` and `uri`) to allow advanced manipulation of further responses based on that information. <u>It is important to highlight that `uri` refers to the received `uri` normalized</u> (having for example, a better predictable query parameters order during server data events search), not the `classification uri` (which could dispense with query parameters or variable parts depending on the matching algorithm), and could also be slightly different in some cases (specially when query parameters are involved) than original `uri` received on HTTP/2 interface.

<u>Without query parameters</u> (`GET /admin/v1/server-data`), you may be careful with large contexts born from long-term tests (load testing), because a huge response could collapse the receiver (terminal or piped process). With query parameters, you could filter a specific entry providing *requestMethod*, *requestUri* and <u>optionally</u> a *eventNumber* and *eventPath*, for example:

`/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3&eventPath=/requestBody`

The `json` document response shall contain three main nodes: `method`, `uri` and a `events` object with the chronologically ordered list of events processed for the given `method/uri` combination.

Both *method* and *uri* shall be provided together (if any of them is missing, a bad request is obtained), and *eventNumber* cannot be provided alone as it is an additional filter which selects the history item for the `method/uri` key (the `events` node will contain a single register in this case). So, the *eventNumber* is the history position, **1..N** in chronological order, and **-1..-N** in reverse chronological order (latest one by mean -1 and so on). The zeroed value is not accepted. Also, *eventPath* has no sense alone and may be provided together with *eventNumber* because it refers to a path within the selected object for the specific position number described before.

This operation is useful for testing post verification stages (validate content and/or document schema for an specific interface). Remember that you could start the *h2agent* providing a requests schema file to validate incoming receptions through traffic interface, but external validation allows to apply different schemas (although this need depends on the application that you are mocking), and also permits to match the requests content that the agent received.

**Important note**: as this operation provides a list of query parameters, and one of these parameters is a *URI* itself (`requestUri`) it may be URL-encoded to avoid ambiguity with query parameters separators ('=', '&'). Use `./tools/url.sh` helper to encode:

`/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&eventNumber=3&eventPath=/requestBody`

Once internally decoded, the request *URI* will be matched against the `uri` <u>normalized</u> as commented above, so encoding must be also done taking this normalization into account (query parameters order).

When provided *method* and *uri*, server data will be filtered with that key. If event number is provided too, the single event object, if exists, will be returned. Same for event path (if nothing found, empty document is returned but status code will be 200, not 204). When no query parameters are provided, the whole internal data organized by key (*method* + *uri*) together with their events arrays are returned.

#### Server event fields

The information collected for a server event item is:

* `virtualOrigin`: special field for virtual entries coming from provisions which established an *out-state* for a foreign method/uri. This entry is necessary to simulate complexes states but you should ignore from the post-verification point of view. The rest of *json* fields will be kept with the original event information, just in case the history is disabled, to allow tracking the maximum information possible. This node holds a `json` nested object containing the `method` and `uri` for the real event which generated this virtual register.
* `receptionTimestampUs`: event reception *timestamp*.
* `state`: working/current state for the event (provision `outState` or target state modified by transformation filters).
* `requestHeaders`: object containing the list of request headers.
* `requestBody`: object containing the request body.
* `previousState`: original provision state which managed this request (provision `inState`).
* `responseBody`: response which was sent.
* `responseDelayMs`: delay which was processed.
* `responseStatusCode`: status code which was sent.
* `responseHeaders`: object containing the list of response headers which were sent.
* `serverSequence`: current server monotonically increased sequence for every reception (`1..N`). In case of a virtual register (if it contains the field `virtualOrigin`), this sequence is actually not increased for the server data entry shown, only for the original event which caused this one.

### Server data summary

`GET /admin/v1/server-data/summary` — when a huge amount of events are stored, we can still troubleshoot an specific known key by mean filtering the server data as commented in the previous section. But if we need just to check what's going on there (imagine a high amount of failed transactions, thus not purged), perhaps some hints like the total amount of receptions or some example keys may be useful to avoid performance impact in the process due to the unfiltered query, as well as difficult forensics of the big document obtained. So, the purpose of server data summary operation is try to guide the user to narrow and prepare an efficient query.

The summary response fields:

* `displayedKeys`: the summary could also be too big to be displayed, so query parameter *maxKeys* will limit the number (`amount`) of displayed keys in the whole response. Each key in the `list` is given by the *method* and *uri*, and also the number of history events (`amount`) is shown.
* `totalEvents`: this includes possible virtual events, although normally this kind of configuration is not usual and the value matches the total number of real receptions.
* `totalKeys`: total different keys (method/uri) registered.

Example:

```json
{
  "displayedKeys": {
    "amount": 3,
    "list": [
      { "amount": 2, "method": "GET", "uri": "/app/v1/foo/bar/1?name=test" },
      { "amount": 2, "method": "GET", "uri": "/app/v1/foo/bar/2?name=test" },
      { "amount": 2, "method": "GET", "uri": "/app/v1/foo/bar/3?name=test" }
    ]
  },
  "totalEvents": 45000,
  "totalKeys": 22500
}
```

### Deleting server data

`DELETE /admin/v1/server-data` deletes the server data given by query parameters defined in the same way as the *GET* operation. For example:

`/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3`

Same restrictions apply here for deletion: query parameters could be omitted to remove everything, *method* and *URI* are provided together and *eventNumber* restricts optionally them.

## Client endpoints

Client endpoints (`POST /admin/v1/client-endpoint`) define remote server connection information where `h2agent` may connect during test execution.

By default, created endpoints will connect the defined remote server (except for lazy connection mode: `--remote-servers-lazy-connection`) but no reconnection procedure is implemented in case of fail. Instead, they will be reconnected on demand when a request is processed through such endpoint.

Mandatory fields are `id`, `host` and `port`. Optional `secure` field is used to indicate the scheme used, *http* (default) or *https*, and `permit` field is used to process (default) or ignore a request through the client endpoint regardless if the connection is established or not (when permitted, a closed connection will be lazily restarted). Using `permit`, flows may be interrupted without having to disconnect the carrier.

Endpoints could be updated through further *POST* requests to the same identifier `id`. When `host`, `port` and/or `secure` are modified for an existing endpoint, connection shall be dropped and re-created again towards the corresponding updated address. In this case, status code *Accepted* (202) will be returned.

Configuration of a set of client endpoints through an array object is allowed:

```json
[
  { "id": "myServer1", "host": "localhost1", "port": 8000 },
  { "id": "myServer2", "host": "localhost2", "port": 8000 }
]
```

A client endpoint set fails with the first failed item, giving a 'pluralized' version of the single configuration failed response message although previous valid client endpoints will be added.

## Client provisions

Client provisions (`POST /admin/v1/client-provision`) are a fundamental part of the client mode configuration. Unlike server provisions, they are identified by the mandatory `id` identifier (in server mode, the primary identifier was the `method/uri` key) and the optional `inState` field (which defaults to "initial" when missing). In client mode, there are no classification algorithms because the provisions are actively triggered through the *REST API*.

### Client state machine

In client mode, the meaning of `inState` is slightly different and represents the evolution for a given <u>identifier understood as specific test scenario</u>: the state shall transition for each of its stages (`outState` dictates the next provision key to be processed).

Example:

* `id`="*scenario1*", `inState`="*initial*", `outState`="*second*"
* `id`="*scenario1*", `inState`="*second*", `outState`="*third*"
* `id`="*scenario1*", `inState`="*third*", `outState`="*purge*"

When *scenario1* is triggered, its current state is searched assuming "initial" when nothing is found in client data storage. So it will be processed and next stage is triggered automatically for the new combination `id` + `outState` when the response is received (timeout is a kind of response but normally user stops the scenario in this case). System test is possible because those stages are replicated by mean different instances of the same scenario evolving separately: this is driven by an internal sequence identifier which is used to calculate real request *method* and *uri*, the ones stored in the data base.

The `outState` holds a reserved default value of `road-closed` for any provision when it is not explicitly configured. This is because here, the provision is not reset and must be guided by the flow execution. This `outState` can be configured on request transformation before sending and after response is received so new flows can be triggered with different stages, but they are unset by default (`road-closed`). This special value is not accepted for `inState` field to guarantee its reserved meaning.

<u>Special **purge** state</u>: same concept as in server provisions. The keyword '*purge*' is a reserved out-state used to indicate that client data related to a scenario (everything for a given `id` and internal sequence) history must be dropped (it should be configured at the last scenario stage provision).

### Client provision fields

#### requestMethod & requestUri

Both can be omitted in the provision, but they are mandatory to be available (so they should be created on transformations) when preparing the request to be sent. The `requestUri` is normally completed/appended by dynamic sequences in order to configure the final *URI* to be sent.

#### requestSchemaId

We could optionally validate built request (after transformations) against a `json` schema. Schemas are identified by string name and configured through [command line](../../README.md#command-line) or REST API. When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

#### requestHeaders

Header fields for the request. For example:

```json
"requestHeaders":
{
  "content-type": "application/json"
}
```

#### requestBody

Request body. Currently supported: object (`json` and arrays), string, integer, number, boolean and null types.

#### requestDelayMs

Optional request delay simulation in milliseconds.

#### timeoutMs

Optional timeout for response in milliseconds. Defaults to 1000 ms (1 second) when not specified, aligned with the underlying `Http2Client::asyncSend` default. A value of 0 is treated as unset and also defaults to 1000 ms.

#### responseSchemaId

We could optionally validate received responses against a `json` schema. When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

#### expectedResponseStatusCode

Optional expected HTTP status code for the response (integer, 100-599). When configured and the received response status code does not match, the provision flow chain is interrupted (no state progression) and a validation failure counter is incremented. This is useful for automated verdict without requiring event storage:

```json
{
  "id": "myFlow",
  "endpoint": "myServer",
  "requestMethod": "GET",
  "requestUri": "/api/v1/resource",
  "expectedResponseStatusCode": 200
}
```

When either `expectedResponseStatusCode` or `responseSchemaId` validation fails:
- The error is logged
- The `h2agent_traffic_client_unexpected_response_status_code_counter` prometheus metric is incremented
- The event is still stored (if storage is enabled) for debugging
- The state progression chain is interrupted (no purge, no next provision)

### Client transformation differences

As in the server mode, we have transformations to be applied, but this time we can transform the context before sending (`transform` node), and when the response is received (`onResponseTransform` node).

Most items are described in the [transformation pipeline](#transformation-pipeline) section. Here, they work in the same way, but there are a few differences:

New **sources**:

- `sendseq`: sequence id number increased for every mock sending over specific client endpoint (starts on *1* when the *h2agent* is started).
- `seq`: sequence id number provided by client provision trigger procedure (we will explain later in [triggering](#triggering)). This value is accessible for every provision processing and is used to create dynamically things like the final request *URI* sent (containing for example, a session identifier) and probably some parts of the request body content. Note that `seq` is the **only** way to access the current sequence value inside a transform — the `dynamics.sequence` field exposed by the REST API is a read-only external snapshot and is **not** available as a transform variable.

New **targets**:

- `request.delayMs` *[unsigned integer]*: simulated delay before sending the request: although you can configure a fixed value for this property on provision document, this transformation target overrides it.
- `request.timeoutMs` *[unsigned integer]*: timeout to wait for the response: although you can configure a fixed value for this property on provision document, this transformation target overrides it.
- `request.uri` *[string]*: overrides the request URI to be sent. This is useful in combination with `seq` source to build dynamic URIs (e.g., `/api/v1/resource/@{myId}`).
- `request.method` *[string]*: overrides the HTTP method to be sent (e.g., `POST`, `GET`, `PUT`, `DELETE`, `HEAD`).
- `break`: this target is activated with non-empty source (for example `value.1`) and interrupts the transformation list. It is used on response context to discard further transformations when, for example, response status code is not valid to continue processing the test scenario. Normally, we should "dirty" the `outState` (for example, setting an unprovisioned "road closed" state, in order to stop the flow) and then break the transformation procedure (this also dodges a probable purge state configured in next stages, keeping internal data for further analysis). Note that placing `break` as the **last** item in a transformation list is illogical — there are no further items to interrupt. A warning is logged at provision time when this is detected.

Note that `clientProvision.<id>.<inState>` target is only available in server mode transformations (not in client mode), as it is the mechanism to trigger client flows from server context.

### Multiple client provisions

Provision of a set of provisions through an array object is allowed:

```json
[
  {
    "id": "test1",
    "endpoint": "myClientEndpoint",
    "requestMethod": "POST",
    "requestUri": "/app/v1/stock/madrid?loc=123",
    "requestBody": { "engine": "tdi", "model": "audi", "year": 2021 },
    "requestHeaders": { "content-type": "application/json" },
    "requestDelayMs": 20,
    "timeoutMs": 2000
  },
  {
    "id": "test2",
    "endpoint": "myClientEndpoint2",
    "requestMethod": "POST",
    "requestUri": "/app/v1/stock/malaga?loc=124",
    "requestBody": { "engine": "hdi", "model": "peugeot", "year": 2023 },
    "requestHeaders": { "content-type": "application/json" },
    "requestDelayMs": 20,
    "timeoutMs": 2000
  }
]
```

A provision set fails with the first failed item, giving a 'pluralized' version of the single provision failed response message although previous valid provisions will be added.

### Unused client provisions

`GET /admin/v1/client-provision/unused` retrieves all the provisions configured that were not used yet. This is useful for troubleshooting (during tests implementation or *SUT* updates) to filter unnecessary provisions configured: when the test is executed, just identify unused items and then remove them from test configuration.
The 'unused' status is initialized at creation time (`POST` operation) or when the provision is overwritten.

### Triggering

To trigger a client provision (`GET /admin/v1/client-provision/<id>`), we will use the *GET* method, providing its identifier in the *URI*.

Normally we shall trigger only provisions for `inState` = "initial" (so, it is the default value when this query parameter is missing). This is because the traffic flow will evolve activating other provision keys given by the <u>same</u> provision identifier but another `inState`. All those internal triggers are indirectly caused by the primal administrative operation which is the only one externally initiated. Although it is possible to trigger an intermediate state (via the `inState` query parameter), this is useful for debugging and also for [partial chain execution](#partial-chain-execution) scenarios.

There are two triggering modes: **synchronous** (single request) and **asynchronous** (timer-based). Their query parameters are mutually exclusive.

#### Synchronous triggering

A single request is sent immediately and the operation returns status code *200*. This is the default behavior when no query parameters are provided (using `sequence` value of '0').

Optionally, the `sequence` query parameter can be provided to set a specific sequence value before sending:

* `sequence`: specific `sequence` value for the request.

This is useful when the provision uses `seq` in its transformations to build dynamic content (e.g., a unique URI or body field). Successive calls with different `sequence` values allow sending varied requests synchronously:

```bash
# Send three requests with sequence values 10, 20 and 30:
for seq in 10 20 30; do
  curl --http2-prior-knowledge "http://localhost:8074/admin/v1/client-provision/myFlow?sequence=${seq}"
done
```

#### Asynchronous triggering

Optional query parameters can be specified to perform multiple triggering (status code *202* is used in operation response instead of *200*). This operation creates internal events sequenced in a range of values (`sequence` variable will be available in provision process for each iterated value) and with specific rate (events per second) to perform system/load tests.

Each client provision can evolve the range of values independently of others, and triggering process may be stopped (with `rps` zero-valued) and then resumed again with a positive rate. Also repeat mode is stored as part of provision trigger configuration with these defaults: range `[0, 0]`, rate of '0' and repeat 'false'.

*Query parameters:*

* `sequenceBegin`: initial `sequence` variable.
* `sequenceEnd`: final `sequence` variable.
* `rps`: rate in requests per second triggered (non-negative value, '0' to stop).
* `repeat`: range repetition once exhausted (true or false).

Negative values are allowed for `sequenceBegin` and `sequenceEnd`, which is useful when a transform applies an offset (e.g. `Sum`) over a central base value:

```bash
# Generate 11 requests with sequence values from -5 to 5 at 100 rps:
curl --http2-prior-knowledge \
  "http://localhost:8074/admin/v1/client-provision/myFlow?sequenceBegin=-5&sequenceEnd=5&rps=100"
```

> **Important**: `sequence` parameter (synchronous) cannot be mixed with `sequenceBegin`, `sequenceEnd`, `rps` or `repeat` (asynchronous). Providing both will result in a *400 Bad Request* error.

So, together with provision information configured, we store dynamic load configuration and state (current `sequence`):

```json
"dynamics": {
  "repeat": false,
  "rps": 1500,
  "sequence": 2994907,
  "sequenceBegin": 0,
  "sequenceEnd": 10000000
}
```

> **Note**: `dynamics` fields are a read-only external snapshot accessible via the REST API only (e.g., to poll for completion). They are **not** available as transform variables. To use the current sequence value inside a transform, use the `seq` source instead.

*Configuration rules:*

- If no query parameters are provided, single event is triggered for `sequence` value of '0'.
- Omitted parameter(s) keeps previous value.
- Provided parameter(s) updates previous value.
- If both `sequenceBegin` and `sequenceEnd` query parameters are present, a single (when coincide) or multiple list of events are created for each `sequence` value.
- Whenever `rps` rate is provided, tick period for request sending is updated (stopped with '0').
- Cycle `repeat` can be updated in any moment, but its effect will be ignored if the range has been completely processed while it was disabled.
- When the range of sequences is completed (`sequenceEnd` reached), trigger configuration is reset and a new administrative operation will be needed.
- Several operations could update load parameters, but `sequence` will evolve if complies with range requirements while rate is positive, so operations could have no effect depending on the information provided.

User may transform sequence value to adapt the test case taking into account that any transformation implemented should be *bijective* towards target set to prevent that values used in the test are repeated or overlapped. For example, we could provide generation range `[0, 99]` to trigger one hundred of *URIs* in the form `/foo/bar/<odd natural numbers>`, just by mean the following transformation item:

```json
{
  "source": "math.2*@{sequence} + 1",
  "filter": { "Prepend": "/foo/bar/" },
  "target": "request.uri"
}
```

Or for example, trigger all the existing values (also even numbers) from `/foo/bar/555000000` to `/foo/bar/555000099`, by mean adding (so padding "in a row") the base number `555000000` to the sequence iterated within the range provided (`[0, 99]`):

```json
[
  {
    "source": "var.sequence",
    "filter": { "Sum": 555000000 },
    "target": "var.phone"
  },
  {
    "source": "value./foo/bar/",
    "filter": { "Append": "@{phone}" },
    "target": "request.uri"
  }
]
```

Note that, in the first transformation item, we are creating a new variable 'phone' because <u>`sequence` variable is reserved and non-writable as target</u> (a warning log is generated when trying to do this).

Also, note that final transformation item uses constant value for source, but it could also use `request.uri` as a source if client provision configures it as `/foo/bar` within provision template.

And finally, note that we could also solve the previous exercise just providing the real range `[555000000, 555000099]` to the operation, processing directly the last single transformation item shown before but appending variable `sequence` instead of `phone`. This is a kind of decision that implies advantages or drawbacks:

* Using ad-hoc ranges saves and simplifies some steps, but you may remember those ranges as part of your testing administrative operations.

* Using standard range `0..N` needs more transformations but shows the real intention within provision programming which are autonomous and ready for use. So testing automation only need to decide the amount of load (`N`) and could mix other provisions already prepared in the same way, which seems easy to coordinate:

  ```bash
  for provision in script1 script2 script3; do # parallel test scripts, 5000 iterations at 200 requests per second:
    curl -i --http2-prior-knowledge http://localhost:8074/admin/v1/client-provision/${provision}?sequenceEnd=4999&rps=200
  done
  ```

#### Partial chain execution

A common pattern is to define a full lifecycle chain (e.g., `initial` → `established` → `updated` → `terminated`) but execute it in stages. This is achieved by combining a global variable with a conditional `break` in the `onResponseTransform` of an intermediate step.

For example, consider a 3-step session flow (create → update → delete). To execute only the establishment phase:

1. Set a global variable to control the chain depth:

   ```bash
   curl --http2-prior-knowledge -d '{"STOP_AFTER": "establishment"}' \
     http://localhost:8074/admin/v1/global-variable
   ```

2. In the `onResponseTransform` of the establishment provision, add a conditional outState override:

   ```json
   {
     "source": "globalVar.STOP_AFTER",
     "target": "var.mustStop",
     "filter": { "RegexCapture": "establishment" }
   },
   {
     "source": "value.road-closed",
     "target": "outState",
     "filter": { "ConditionVar": "mustStop" }
   }
   ```

   When `STOP_AFTER` matches `"establishment"`, the `outState` is overridden to an unprovisioned state (`road-closed`), preventing automatic state progression. The chain simply stops because no provision exists for `(mySession, road-closed)`.

3. Later, to complete the remaining steps (update + delete), trigger from the intermediate state:

   ```bash
   # First, clear the stop condition:
   curl --http2-prior-knowledge -d '{"STOP_AFTER": ""}' \
     http://localhost:8074/admin/v1/global-variable

   # Then trigger from the intermediate state:
   curl --http2-prior-knowledge \
     "http://localhost:8074/admin/v1/client-provision/mySession?inState=established&sequenceBegin=0&sequenceEnd=999&rps=500"
   ```

> **Note**: when triggering from an intermediate state, scoped variables (`var.*`) captured in earlier steps are only available if the chain was previously executed through those steps (variables propagate along `outState` links). If you trigger an intermediate state directly, ensure the provision does not depend on variables from prior steps — or use `globalVar` instead.

This pattern is useful for benchmarking scenarios where you want to measure each phase independently, or when you need to pre-populate sessions before running update/delete workloads.

#### Server-triggered client flows

Client provisions can also be triggered from within server provision transformations using the `clientProvision.<clientProvisionId>.<inState>` target (described in the [transformation pipeline](#transformation-pipeline) section). This allows the server to react to an incoming request by firing one or more outgoing client flows as a side effect. The source acts as a conditional gate: non-empty fires the trigger, empty or `eraser` skips it.

```json
{
  "requestMethod": "POST",
  "requestUri": "/webhook/notify",
  "responseCode": 200,
  "transform": [
    {
      "source": "value.1",
      "target": "clientProvision.forwardNotification.initial"
    }
  ]
}
```

The triggers are executed asynchronously after the server response is fully built, so they do not affect server response latency. Multiple `clientProvision` targets can coexist in the same transformation list.

## Client data

### Client storage configuration

Same explanation done for `server-data` equivalent operation, applies here (`PUT /admin/v1/client-data/configuration`). Just to know that history events here have an extended key adding `client endpoint id` to the `method` and `uri` processed. The purge procedure is performed over the specific provision identifier, removing everything registered for any working `state` and for the current processed `sequence` value.

The same agent could manage server and client connections, so you have specific configurations for internal data regarding server or client events, but normally, we shall use only one mode to better separate responsibilities within the testing ecosystem.

`GET /admin/v1/client-data/configuration` retrieves the current configuration:

```json
{
    "needsStorage": false,
    "purgeExecution": true,
    "storeEvents": true,
    "storeEventsKeyHistory": true
}
```

By default, the `h2agent` enables both kinds of storage types (general events and requests history events), and also enables the purge execution if any provision with this state is reached, so the previous response body will be returned on this query operation. This is useful for function/component testing where more information available is good to fulfill the validation requirements. In load testing, we could seize the `purge` out-state to control the memory consumption, or even disable storage flags in case that test plan is stateless and allows to do that simplification.

### Querying client data

`GET /admin/v1/client-data` retrieves the current client internal data (requests sent, their provision identifiers, states and other useful information like timing or global order). By default, the `h2agent` stores the whole history of events (for example requests sent for the same `clientEndpointId`, `method` and `uri`) to allow advanced manipulation of further responses based on that information. <u>It is important to highlight that `uri` refers to the final sent `uri` normalized</u> (having for example, a better predictable query parameters order during client data events search), not necessarily the `provisioned uri` within the provision template.

<u>Without query parameters</u> (`GET /admin/v1/client-data`), you may be careful with large contexts born from long-term tests (load testing), because a huge response could collapse the receiver (terminal or piped process). With query parameters, you could filter a specific entry providing *clientEndpointId*, *requestMethod*, *requestUri* and <u>optionally</u> a *eventNumber* and *eventPath*, for example:

`/admin/v1/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3&eventPath=/responseBody`

The `json` document response shall contain three main nodes: `clientEndpointId`, `method`, `uri` and a `events` object with the chronologically ordered list of events processed for the given `clientEndpointId/method/uri` combination.

Both *clientEndpointId*, *method* and *uri* shall be provided together (if any of them is missing, a bad request is obtained), and *eventNumber* cannot be provided alone as it is an additional filter which selects the history item for the `clientEndpointId/method/uri` key (the `events` node will contain a single register in this case). So, the *eventNumber* is the history position, **1..N** in chronological order, and **-1..-N** in reverse chronological order (latest one by mean -1 and so on). The zeroed value is not accepted. Also, *eventPath* has no sense alone and may be provided together with *eventNumber* because it refers to a path within the selected object for the specific position number described before.

This operation is useful for testing post verification stages (validate content and/or document schema for an specific interface). Remember that you could start the *h2agent* providing a response schema file to validate incoming responses through traffic interface, but external validation allows to apply different schemas (although this need depends on the application that you are mocking).

**Important note**: as this operation provides a list of query parameters, and one of these parameters is a *URI* itself (`requestUri`) it may be URL-encoded to avoid ambiguity with query parameters separators ('=', '&'). Use `./tools/url.sh` helper to encode:

`/admin/v1/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&eventNumber=3&eventPath=/responseBody`

#### Client event fields

The information collected for a client event item is:

* `clientProvisionId`: provision identifier.
* `clientSequence`: current client monotonically increased sequence for every sending (`1..N`).
* `sendingTimestampUs`: event sending *timestamp* (request).
* `receptionTimestampUs`: event reception *timestamp* (response).
* `state`: working/current state for the event (provision `outState` or target state modified by transformation filters).
* `requestHeaders`: object containing the list of request headers.
* `requestBody`: object containing the request body.
* `previousState`: original provision state which managed this request (provision `inState`).
* `responseBody`: response which was received.
* `requestDelayMs`: delay for outgoing request.
* `responseStatusCode`: status code which was received.
* `responseHeaders`: object containing the list of response headers which were received.
* `sequence`: internal provision sequence.
* `timeoutMs`: accepted timeout for request response.

### Client data summary

`GET /admin/v1/client-data/summary` — when a huge amount of events are stored, we can still troubleshoot an specific known key by mean filtering the client data as commented in the previous section. But if we need just to check what's going on there (imagine a high amount of failed transactions, thus not purged), perhaps some hints like the total amount of sendings or some example keys may be useful to avoid performance impact in the process due to the unfiltered query, as well as difficult forensics of the big document obtained. So, the purpose of client data summary operation is try to guide the user to narrow and prepare an efficient query.

The summary response fields:

* `displayedKeys`: query parameter *maxKeys* will limit the number (`amount`) of displayed keys. Each key in the `list` is given by the *clientEndpointId*, *method* and *uri*, and also the number of history events (`amount`) is shown.
* `totalEvents`: total number of events.
* `totalKeys`: total different keys (clientEndpointId/method/uri) registered.

Example:

```json
{
  "displayedKeys": {
    "amount": 3,
    "list": [
      { "amount": 2, "clientEndpointId": "myClientEndpointId", "method": "GET", "uri": "/app/v1/foo/bar/1?name=test" },
      { "amount": 2, "clientEndpointId": "myClientEndpointId", "method": "GET", "uri": "/app/v1/foo/bar/2?name=test" },
      { "amount": 2, "clientEndpointId": "myClientEndpointId", "method": "GET", "uri": "/app/v1/foo/bar/3?name=test" }
    ]
  },
  "totalEvents": 45000,
  "totalKeys": 22500
}
```

### Deleting client data

`DELETE /admin/v1/client-data` deletes the client data given by query parameters defined in the same way as the *GET* operation. For example:

`/admin/v1/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3`

Same restrictions apply here for deletion: query parameters could be omitted to remove everything, *clientEndpointId*, *method* and *URI* are provided together and *eventNumber* restricts optionally them.
