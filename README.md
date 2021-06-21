# C++ HTTP/2 Agent Service

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://codedocs.xyz/testillano/h2agent.svg)](https://codedocs.xyz/testillano/h2agent/index.html)
[![Ask Me Anything !](https://img.shields.io/badge/Ask%20me-anything-1abc9c.svg)](https://github.com/testillano)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/testillano/h2agent/graphs/commit-activity)
[![Publish docker images and helm charts](https://github.com/testillano/h2agent/actions/workflows/publish.yml/badge.svg)](https://github.com/testillano/h2agent/actions/workflows/publish.yml)

`h2agent` is a network service that enables mocking other network services.

When developing a network service, one often needs to integrate it with other services. However, integrating full-blown versions of such services in a development setup is not always suitable, for instance when they are either heavyweight or not fully developed.

`h2agent` can be used to replace one of those, which allows development to progress and testing to be conducted in isolation against such a service.

`h2agent` supports HTTP2 as a network protocol and JSON as a data interchange language.

There is a *Prezi* presentation [here](https://prezi.com/view/RFaiKzv6K6GGoFq3tpui/).

## Project image

This image is already available at `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull testillano/h2agent:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$ ./build.sh --project-image
```

This image is built with `./Dockerfile`.

## Usage

The static-autonomous executable docker image, will be also available through corresponding `helm charts` (normally packaged into releases) which will be described in following [sections](#how-it-is-delivered).

## Build project with docker

### Builder image

This image is already available at `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull testillano/h2agent_builder:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$ ./build.sh --builder-image
```

This image is built with `./Dockerfile.build`.

### Usage

Builder image is used to build the executable. To run compilation over this image, again, just run with `docker`:

```bash
$ envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         testillano/h2agent_builder:<tag>
```

You could generate documentation passing extra arguments to the [entry point](https://github.com/testillano/nghttp2/blob/master/deps/build.sh) behind:

```bash
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         testillano/h2agent_builder::<tag> "" doc
```

You could also build the library using the script `./build.sh` located at project root:


```bash
$ ./build.sh --project
```

## Build project natively

This is a cmake-based building library, so you may install cmake:

```bash
$ sudo apt-get install cmake
```

And then generate the makefiles from project root directory:

```bash
$ cmake .
```

You could specify type of build, 'Debug' or 'Release', for example:

```bash
$ cmake -DCMAKE_BUILD_TYPE=Debug .
$ cmake -DCMAKE_BUILD_TYPE=Release .
```

You could also change the compilers used:

```bash
$ cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++     -DCMAKE_C_COMPILER=/usr/bin/gcc
```

or

```bash
$ cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang
```

### Requirements

Check the requirements described at building `dockerfile` (`./Dockerfile.build`) as well as all the ascendant docker images which are inherited:

```
h2agent builder (./Dockerfile.build)
   |
http2comm (https://github.com/testillano/http2comm)
   |
nghttp2 (https://github.com/testillano/nghttp2)
```

### Build

```bash
$ make
```

### Clean

```bash
$ make clean
```

### Documentation

```bash
$ make doc
```

```bash
$ cd docs/doxygen
$ tree -L 1
     .
     ├── Doxyfile
     ├── html
     ├── latex
     └── man
```

### Install

```bash
$ sudo make install
```

Optionally you could specify another prefix for installation:

```bash
$ cmake -DMY_OWN_INSTALL_PREFIX=$HOME/myPrograms/http2
$ make install
```

### Uninstall

```bash
$ cat install_manifest.txt | sudo xargs rm
```

## Testing

### Unit test

TODO

### Component test

Component test is based in `pytest` framework. Just execute `ct/test.sh` to deploy the component test chart. Some cloud-native technologies are required: `docker`, `kubectl`, `minikube` and `helm`, for example:

```bash
$ docker version
Client: Docker Engine - Community
 Version:           19.03.11
 API version:       1.40
 Go version:        go1.13.10
 Git commit:        42e35e61f3
 Built:             Mon Jun  1 09:12:22 2020
 OS/Arch:           linux/amd64
 Experimental:      false

Server: Docker Engine - Community
 Engine:
  Version:          19.03.11
  API version:      1.40 (minimum version 1.12)
  Go version:       go1.13.10
  Git commit:       42e35e61f3
  Built:            Mon Jun  1 09:10:54 2020
  OS/Arch:          linux/amd64
  Experimental:     false
 containerd:
  Version:          1.4.3
  GitCommit:        269548fa27e0089a8b8278fc4fc781d7f65a939b
 runc:
  Version:          1.0.0-rc92
  GitCommit:        ff819c7e9184c13b7c2607fe6c30ae19403a7aff
 docker-init:
  Version:          0.18.0
  GitCommit:        fec3683

$ kubectl version
Client Version: version.Info{Major:"1", Minor:"18", GitVersion:"v1.18.9", GitCommit:"94f372e501c973a7fa9eb40ec9ebd2fe7ca69848", GitTreeState:"clean", BuildDate:"2020-09-16T13:56:40Z", GoVersion:"go1.13.15", Compiler:"gc", Platform:"linux/amd64"}
Server Version: version.Info{Major:"1", Minor:"18", GitVersion:"v1.18.9", GitCommit:"94f372e501c973a7fa9eb40ec9ebd2fe7ca69848", GitTreeState:"clean", BuildDate:"2020-09-16T13:47:43Z", GoVersion:"go1.13.15", Compiler:"gc", Platform:"linux/amd64"}

$ minikube version
minikube version: v1.12.3
commit: 2243b4b97c131e3244c5f014faedca0d846599f5-dirty

$ helm version
version.BuildInfo{Version:"v3.3.3", GitCommit:"55e3ca022e40fe200fbc855938995f40b2a68ce0", GitTreeState:"clean", GoVersion:"go1.14.9"}
```
## Execution

### Command line

You may take a look to `h2agent` command line by just typing `./h2agent -h|--help`:

```
./h2agent -h
Usage: h2agent [options]

Options:

[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]
  Set the logging level; defaults to warning.

[--verbose]
  Output log traces on console.

[-a|--admin-port <port>]
  Admin <port>; defaults to 8074.

[-p|--server-port <port>]
  Server <port>; defaults to 8000.

[-m|--server-api-name <name>]
  Server API name; defaults to empty.

[-n|--server-api-version <version>]
  Server API version; defaults to empty.

[-w|--worker-threads <threads>]
  Number of worker threads; defaults to -1 which means 'dynamically created'.
  For high loads, a queue of pre-initialized threads could improve performance
   and its pool size corresponds quite a lot with the client concurrent streams.

[-t|--server-threads <threads>]
  Number of nghttp2 server threads; defaults to 1 (1 connection).

[-k|--server-key <path file>]
  Path file for server key to enable SSL/TLS; unsecured by default.

[--server-key-password <password>]
  When using SSL/TLS this may provided to avoid 'PEM pass phrase' prompt at process
   start.

[-c|--server-crt <path file>]
  Path file for server crt to enable SSL/TLS; unsecured by default.

[-s|--secure-admin]
  When key (-k|--server-key) and crt (-c|--server-crt) are provided, only the traffic
   interface is secured by default. To include management interface, this option must
   be also provided.

[--server-request-schema <path file>]
  Path file for the server schema to validate requests received.

[--disable-server-requests-history]
  Disables full history storage for requests received (enabled by default).
  Only latest request (for each key 'method/uri') will be stored and will
   be accessible for further analysis. To be considered if really makes an
   improvement in huge long-term stabilities.

[-v|--version]
  Program version.

[-h|--help]
  This help.
```

### Traces and printouts

Traces are managed by `syslog` by default, but could be shown verbosely at standard output (`--verbose`) depending on the traces design level and the current level assigned. For example:

```bash
$ ./h2agent --verbose &
[1] 27407
[03/04/21 20:49:35 CEST] Starting h2agent (version v0.0.1-27-g04c11e9) ...
Log level: Warning
Verbose (stdout): true
Admin port: 8074
Server port: 8000
Server api name: <none>
Server api version: <none>
Worker threads: dynamically created
Server threads: 1
Server key password: <not provided>
Server key file: <not provided>
Server crt file: <not provided>
SSL/TLS disabled: both key & certificate must be provided
Traffic secured: no
Admin secured: no
Server request schema: <not provided>
Server request history: true

$ kill $!
[Warning]|/code/src/main.cpp:114(sighndl)|Signal received: 15
[Warning]|/code/src/main.cpp:104(_exit)|Terminating with exit code 1
[Warning]|/code/src/main.cpp:90(stopServers)|Stopping h2agent admin service at 03/04/21 20:49:40 CEST
[Warning]|/code/src/main.cpp:97(stopServers)|Stopping h2agent mock service at 03/04/21 20:49:40 CEST
[1]+  Exit 1                  h2agent --verbose
```

## Management interface

`h2agent` listens on a specific management port (*8074* by default) for incoming requests, implementing a *REST API* to manage the process operation. Through the *API* we could program the agent behavior. The following sections describe all the supported operations over *URI* path`/provision/v1/`:

**Current development phase is 1**, see [Implementation Strategy](#implementation-strategy).

### POST /provision/v1/server-initialize

Initializes the server endpoint for the provided listen port (mandatory).
Not implemented in *Phase 1*, see [Implementation Strategy](#implementation-strategy).

#### Request body schema

`POST` request must comply the following schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "serverPort": {
      "type": "integer"
    },
    "requestSchema": {
      "oneOf": [
        {"type": "object"},
        {"type": "string"}
      ]
    }
  },
  "required": [ "serverPort" ]
}
```

The `json` schema provided through `requestSchema` field object, will be used to validate requests received by `h2agent` server endpoint (you could constraint specific values with `"const"` from `json` schema [draft 6](http://json-schema.org/draft/2019-09/json-schema-validation.html#rfc.section.6.1.3)). This schema is optional, so it is possible to accept incoming requests without any kind of restriction for them.

During *Phase 1* the request schema is passed through [command line](#command-line) by mean `--server-request-schema` option.

#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### POST /provision/v1/server-matching

Defines the server matching procedure for incoming receptions on mock service. Every *URI* received is matched depending on the selected algorithm.

#### Request body schema

`POST` request must comply the following schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "algorithm": {
      "type": "string",
        "enum": ["FullMatching", "FullMatchingRegexReplace", "PriorityMatchingRegex"]
    },
    "rgx": {
      "type": "string"
    },
    "fmt": {
      "type": "string"
    },
    "uriPathQueryParametersFilter": {
      "type": "string",
        "enum": ["Sort", "SortSemicolon", "PassBy", "Ignore"]
    }
  },
  "required": [ "algorithm" ]
}
```

##### uriPathQueryParametersFilter

Optional argument used to specify the transformation for query parameters received in the *URI* path.

###### Sort

This is the default behavior and consists in sorting received query parameters keys using *ampersand* (`'&'`) as separator for key-value pairs. Provisions will be more predictable as input does.

###### SortSemicolon

Same as Sort, but using *semicolon* (`';'`) as query parameters pairs separator.

###### PassBy

If received, query parameters are kept without modifying the received *URI* path.

###### Ignore

If received, query parameters are ignored (removed from *URI* path and not taken into account to match provisions).

##### rgx & fmt

Optional arguments used in `FullMatchingRegexReplace` algorithm.

##### algorithm

###### FullMatching

Arguments `rgx`and `fmt` are not used here, so not allowed (to enforce user experience). The incoming request is fully translated into key without any manipulation, and then searched in internal provision map.

This is the default algorithm. Internal provision is stored in a map indexed with real requests information to compose an aggregated key (normally containing the requests *method* and *URI*, but as future proof, we could add `expected request` fingerprint). Then, when a request is received, the map key is calculated and retrieved directly to be processed.

This algorithm is very good and easy to use for predictable functional tests (as it is accurate), also giving internally  better performance for provision selection.

###### FullMatchingRegexReplace

Both `rgx` and `fmt` arguments are required. This algorithm is based in [regex-replace](http://www.cplusplus.com/reference/regex/regex_replace/) transformation. The first one (*rgx*) is the matching regular expression, and the second one (*fmt*) is the format specifier string which defines the transformation. For example, you could trim an *URI* received in different ways:

`URI` example:

```
uri = "ctrl/v2/id-555112233/ts-1615562841"
```

* Remove last *timestamp* path part (`ctrl/v2/id-555112233/`):

```
rgx = "(ctrl/v2/id-[0-9]+/)(ts-[0-9]+)"
fmt = "$1"
```

* Trim last four digits (`ctrl/v2/id-555112233/ts-161556`):

```
rgx = "(ctrl/v2/id-[0-9]+/ts-[0-9]+)[0-9]{4}"
fmt = "$1"
```

So, this `regex-replace` algorithm is flexible enough to cover many possibilities (even *tokenize* path query parameters). As future proof, other fields could be added, like algorithm flags defined in underlying C++ `regex` standard library used.

The previous *full matching* algorithm could be simulated here using empty strings for `rgx` and `fmt`, but having obviously a performance degradation.

###### PriorityMatchingRegex

Arguments `rgx`and `fmt` are not used here, so not allowed (to enforce user experience). This identification algorithm relies in the original provision order to match the receptions and reach the first valid occurrence. For example, consider 3 provision operations which are provided sequentially in the following order:

1. `ctrl/v2/id-55500[0-9]{4}/ts-[0-9]{10}`
2. `ctrl/v2/id-5551122[0-9]{2}/ts-[0-9]{10}`
3. `ctrl/v2/id-555112244/ts-[0-9]{10}`

If the `URI` "*ctrl/v2/id-555112244/ts-1615562841*" is received, the second one is the first positive match and then, selected to mock the provisioned answer. Even being the third one more accurate, this algorithm establish an ordered priority to match the information.

As provision key is built combining *inState*, *method* and *uri* fields, a regular expression could also be provided for *inState* (*method* is strictly checked), although this is not usual.

#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### GET /provision/v1/server-matching/schema

Retrieves the server matching schema.

#### Response status code

**200** (OK).

#### Response body

Json document containing server matching schema.

### GET /provision/v1/server-matching

Retrieves the current server matching configuration.

#### Response status code

**200** (OK).

#### Response body

Json document containing server matching information, '*null*' if nothing configured (working with defaults).

### POST /provision/v1/server-provision

Defines the response behavior for an incoming request matching some basic conditions (*method*, *uri*) and programming the response (*header*, *code*, *body*).

#### Request body schema

`POST` request must comply the following schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "definitions": {
    "filter": {
      "type": "object",
      "additionalProperties": false,
      "oneOf": [
        {"required": ["RegexCapture"]},
        {"required": ["RegexReplace"]},
        {"required": ["Append"]},
        {"required": ["Prepend"]},
        {"required": ["Sum"]},
        {"required": ["Multiply"]},
        {"required": ["ConditionVar"]}
      ],
      "properties": {
        "RegexCapture": { "type": "string" },
        "RegexReplace": {
          "type": "object",
          "additionalProperties": false,
          "properties": {
            "rgx": {
              "type": "string"
            },
            "fmt": {
              "type": "string"
            }
          },
          "required": [ "rgx", "fmt" ]
        },
        "Append": { "type": "string" },
        "Prepend": { "type": "string" },
        "Sum": { "type": "number" },
        "Multiply": { "type": "number" },
        "ConditionVar": { "type": "string" }
      }
    }
  },
  "type": "object",
  "additionalProperties": false,

  "properties": {
    "inState":{
      "type": "string"
    },
    "outState":{
      "type": "string"
    },
    "requestMethod": {
      "type": "string",
        "enum": ["POST", "GET", "PUT", "DELETE", "HEAD"]
    },
    "requestUri": {
      "type": "string"
    },
    "responseHeaders": {
      "additionalProperties": {
        "type": "string"
       },
       "type": "object"
    },
    "responseCode": {
      "type": "integer"
    },
    "responseBody": {
      "type": "object"
    },
    "responseDelayMs": {
      "type": "integer"
    },
    "transform" : {
      "type" : "array",
      "minItems": 1,
      "items" : {
        "type" : "object",
        "minProperties": 2,
        "maxProperties": 3,
        "properties": {
          "source": {
            "type": "string",
            "pattern": "^var\\..+|^value\\..*|^request\\.uri$|^request\\.uri\\.path$|^request\\.uri\\.param\\..+|^request\\.body$|^request\\.body\\..+|^request\\.header\\..+|^general\\.random\\.[-+]{0,1}[0-9]+\\.[-+]{0,1}[0-9]+$|^general\\.timestamp\\.[m|n]{0,1}s$|^general\\.strftime\\..+|^general\\.recvseq$|^inState$"
          },
          "target": {
            "type": "string",
            "pattern": "^var\\..+|^response\\.body(\\.object$|\\.jsonstring$|\\.string$|\\.integer$|\\.unsigned$|\\.float$|\\.boolean$)?(\\..)+|^response\\.header\\..+|^response(\\.statusCode$|\\.delayMs$)|^outState(\\.POST|\\.GET|\\.PUT|\\.DELETE|\\.HEAD)?$"
          }
        },
        "additionalProperties" : {
          "$ref" : "#/definitions/filter"
        },
        "required": [ "source", "target" ]
      }
    }
  },
  "required": [ "requestMethod", "responseCode" ]
}
```

##### inState and outState

We could label a provision specification to take advantage of internal *FSM* (finite state machine) for matched occurrences. When a reception matches a provision specification, the real context is searched internally to get the current state ("**initial**" if missing) and then get the  `inState` provision for that value. Then, the specific provision is processed and the new state will get the `outState` provided value. This makes possible to program complex flows which depends on some conditions, not only related to matching keys, but also consequence from [transformation filters](#transform) which could manipulate those states.

These arguments are configured by default with the label "**initial**", used by the system when a reception does not match any internal occurrence (as the internal state is unassigned). This conforms a default rotation for further occurrences because the `outState` is again the next `inState`value. It is important to understand that if there is not at least 1 provision with `inState` = "**initial**" the matched occurrences won't never be processed. Also, if the next state configured (`outState` provisioned or transformed) has not a corresponding `inState` value, the flow will be broken/stopped.

Let's see an example to clarify:

* Provision *X* (match m, `inState`="*initial*"): `outState`="*second*", `response` *XX*
* Provision *Y* (match m, `inState`="*second*"): `outState`="*initial*", `response` *YY*
* Reception matches *m* and internal context map is empty: as we assume state "*initial*", we look for this  `inState` value for match *m*, which is provision *X*.
* Response *XX* is sent. Internal state will take the provision *X* `outState`, which is "*second*".
* Reception matches *m* and internal context map stores state "*second*", we look for this  `inState` value for match *m*, which is provision Y.
* Response *YY* is sent. Internal state will take the provision *Y* `outState`, which is "*initial*".

Further similar matches (*m*), will repeat the cycle again and again.

##### requestMethod

Expected request method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*).

##### requestUri

Request *URI* path (percent-encoded) to match depending on the algorithm selected. It includes possible query parameters, depending on matching filters provided for them.

<u>*Empty string is accepted*</u>, and is reserved to configure an optional default provision, something which could be specially useful to define the fall back provision if no matching entry is found. So, you could configure defaults for any method received, just putting an empty *request URI* or omitting this optional field.

##### responseHeaders

Header fields for the response. For example:

```json
"responseHeaders":
{
  "content-type": "application/json"
}
```



##### responseCode

Response status code.

##### responseBody

Response body.

##### responseDelayMs

Optional response delay simulation in milliseconds.

##### transform

Sorted list of transformations to modify incoming information and build the dynamic response to be sent.

Each transformation has a `source`, a `target` and an optional `filter` algorithm. <u>The filters are applied over sources and sent to targets</u> (all the available filters at the moment act over sources in string format, so they need to be converted if they are not strings in origin).

A best effort is done to transform and convert information to final target vaults, and when something wrong happens, a logging error is thrown and the transformation filter is skipped to move to the next one to be processed. For example, a source detected as *json* object cannot be assigned to a number or string target, but could be set into another *json* object.

Let's start describing the available sources of data: regardless the native or normal representation for every kind of target, the fact is that conversions may be done to almost every other type:

- *string* to *number* and *boolean* (true if non empty).

- *number* to *string* and *boolean* (true if different than zero).

- *boolean*: there is no source for boolean type, but you could create a non-empty string or non-zeroed number to represent *true* on a boolean target (only response body nodes could include a boolean).

- *json object*: request body node (whole document is indeed the root node) when being an object itself (note that it may be also a number or string). This data type can only be transfered into targets which support json objects like response body node.



The **source** of information is classified after parsing the following possible expressions:

- request.uri: whole request *URI*  path, including the possible query parameters.
- request.uri.path: request *URI* path part.
- request.uri.param.<name>: request URI specific parameter `<name>`.
- request.body: request body document from root.
- request.body.<node1>..<nodeN>: request body node path.
- request.header.<hname>: request header component (i.e. *content-type*).
- general.random.<min>.<max>: integer number in range `[min, max]`. Negatives allowed, i.e.: `"-3.+4"`.
- general.timestamp.<unit>: UNIX epoch time in `s` (seconds), `ms` (milliseconds) or `ns` (nanoseconds).
- general.strftime.<format>: current date/time formatted by [strftime](https://www.cplusplus.com/reference/ctime/strftime/).
- general.recvseq: sequence id number increased for every mock reception (starts on *0* when the *h2agent*  is started).
- var.<id>: general purpose variable. Cannot refer json objects.
- value.<value>: free string value. Even convertible types are allowed, for example: integer string, unsigned integer string, float number string, boolean string (true if non-empty string), will be converted to the target type. Empty value is allowed, for example, to set an empty string, just type: `"value."`.
- inState: current processing state.



The **target** of information is classified after parsing the following possible expressions (between *[square brackets]* we denote the potential data types allowed):

- response.body.string *[string]*: response body document storing expected string.

- response.body.integer *[integer]*: response body document storing expected integer.

- response.body.unsigned *[unsigned integer]*: response body document storing expected unsigned integer.

- response.body.float *[float number]*: response body document storing expected float number.

- response.body.boolean *[boolean]*: response body document storing expected boolean.

- response.body.object *[json object]*: response body document storing expected object as root node.

- response.body.jsonstring *[json string]*: response body document storing expected object, extracted from json-parsed string, as root node.

- response.body.string.<node1>..<nodeN> *[string]*: response body node path storing expected string.

- response.body.integer.<node1>..<nodeN> *[integer]*: response body node path storing expected integer.

- response.body.unsigned.<node1>..<nodeN> *[unsigned integer]*: response body node path storing expected unsigned integer.

- response.body.float.<node1>..<nodeN> *[float number]*: response body node path storing expected float number.

- response.body.boolean.<node1>..<nodeN> *[boolean]*: response body node path storing expected booblean.

- response.body.object.<node1>..<nodeN> *[json object]*: response body node path storing expected object under provided path. If source origin is not an object, there will be a best effort to convert to string, number, unsigned number, float number and boolean, in this specific priority order.

- response.body.jsonstring.<node1>..<nodeN> *[json string]*: response body node path storing expected object, extracted from json-parsed string, under provided path.

- response.header.<hname> *[string (or number as string)]*: response header component (i.e. *location*).

- response.statusCode *[unsigned integer]*: response status code.

- response.delayMs *[unsigned integer]*: simulated delay to respond: although you can configure a fixed value for this property on provision document, this transformation target overrides it.

- var.<id> *[string (or number as string)]*: general purpose variable (intended to be used as source later). The idea of *variable* vaults is to optimize transformations when multiple transfers are going to be done (for example, complex operations like regular expression filters, are dumped to a variable, and then, we drop its value over many targets without having to repeat those complex algorithms again). Cannot store json objects.

- outState *[string (or number as string)]*: next processing state. This overrides the default provisioned one.

- outState.[POST|GET|PUT|DELETE|HEAD] *[string (or number as string)]*: next processing state for specific method (virtual server data will be created if needed: this way we could modify the flow for other methods different than the one which is managing the current provision).

  You could, for example, simulate a database where a *DELETE* for an specific entry could infer through its provision an *out-state* for a foreign method like *GET*, so when getting that *URI* you could obtain a *404* (assumed this provision for the new *working-state* = *in-state* = *out-state* = "id-deleted").

  This overrides the default provisioned one.



There are several **filter** methods, but remember that filter node is optional, so you could directly transfer source to target without modification, just omitting filter, for example:

```json
{
  "source": "general.random.25.35",
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
     "filter": { "RegexCapture" : "api\/v2\/id-([0-9]+)\/category-([a-z]+)" }
   }
   ```

   In this case, if the source received is *"api/v2/id-28/category-animal/"*, then we have 2 captured groups, so, we will have: *var.id_cat.1="28"* and *var.id_cat.2="animal"*. Also, the specified variable name *"as is"* will store the entire match: *var.id_cat="api/v2/id-28/category-animal/"*.

   Other example:

  ```json
  {
    "source": "request.uri.path",
    "target": "response.body.string.category",
    "filter": { "RegexCapture" : "api\/v2\/id-[0-9]+\/category-([a-z]+)" }
  }
  ```

  In this example, it is not important to notice that we only have 1 captured group (we removed the brackets of the first one from the previous example). This is because the target is a path within the response body, not a variable, so, only the entire match (if proceed) will be transferred. Assuming we receive the same source from previous example, that value will be the entire *URI* path. If we would use a variable as target, such variable would store the same entire match, and also we would have *animal* as `<variable name>.1`.

  If you want to move directly the captured group (`animal`) to a non-variable target, you may use the next filter:



- RegexReplace: this is similar to the matching algorithm based in regular expressions and replace procedure. We provide `rgx` and `fmt` to transform the source into the target:

  ```json
  {
    "source": "request.uri.path",
    "target": "response.body.unsigned.data.timestamp",
    "filter": {
      "RegexReplace" : {
        "rgx" : "(ctrl/v2/id-[0-9]+/)ts-([0-9]+)",
        "fmt" : "$2"
      }
    }
  }
  ```

  For example, if the source received is "*ctrl/v2/id-555112233/ts-1615562841*", then we will replace/create a node "*data.timestamp*" within the response body, with the value formatted: *1615562841*.

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




- Append: this appends the provided information to the source:

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



- Prepend: this prepends the provided information to the source:

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



- Sum: adds the source (if numeric conversion is possible) to the value provided (which <u>also could be negative or float</u>):

  ```json
  {
    "source": "general.random.0.99999999",
    "target": "var.mysum",
    "filter": { "Sum" : 123456789012345 }
  }
  ```

  In this example, the random range limitation (integer numbers) is uncaged through the addition operation. Using this together with other filter algorithms should complete most of the needs.



- Multiply: multiplies the source (if numeric conversion is possible) by the value provided (which <u>also could be negative to change sign, or lesser than 1 to divide</u>):

  ```json
  {
    "source": "value.-10",
    "target": "var.value-of-one",
    "filter": { "Multiply" : -0.1 }
  }
  ```

  In this example, we operate `-10 * -0.1 = 1`.



- ConditionVar: conditional transfer from source to target based in boolean interpretation of the provided variable value. If the variable is not defined, the condition is false. All the variables are strings in origin, and are converted to target selected types, but in this case the variable value is adapted as string to the boolean result following this procedure: empty string means *false*, any other situation is *true* (note that a variable containing the literal "false" would be interpreted as *true*). This decision allows to use, directly, regular expressions matches as booleans (a target variable stores the source match when using *RegexCapture* filter).

  ```json
  {
    "source": "value.500",
    "target": "response.statusCode",
    "filter": { "ConditionVar" : "transfer-500-to-status-code" }
  }
  ```

  The variable used for the condition should be defined in a previous transformation, for example:

  ```json
  {
    "source": "request.body.forceErrors.internalServerError",
    "target": "var.transfer-500-to-status-code"
  }
  ```

  In this example, the request body dictates the responses' status code to receive in the node path "*/forceErrors/internalServerError*". Of course there are many ways to set the condition variable depending on the needs.



#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### GET /provision/v1/server-provision/schema

Retrieves the server provision schema.

#### Response status code

**200** (OK).

#### Response body

Json document containing server provision schema.

### GET /provision/v1/server-provision

Retrieves the current provision map.

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

Json array containing all provisioned items, '*null*' if nothing configured.

### DELETE /provision/v1/server-provision

Deletes the current provision. It is useful to clear the configuration if the provisioned data collides between different test cases and need to be reset.

This operation also removes server mock internal data (requests and their states) as this only have sense for the provision which consolidated such information.

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

No response body.

### POST provision/v1/server-data/schema

Loads a requests schema for validation of traffic receptions.

#### Request body

Request body will be the `json` schema for the requests.

#### Response status code

**201** (Created) or **400** (Bad Request).

### GET /provision/v1/server-data/schema

Retrieves the server requests schema if configured (at command-line).

#### Response status code

**200** (OK), **204** (No Content).

#### Response body

Json document containing server requests schema if configured, or empty if not.

### GET /provision/v1/server-data

Retrieves the current server internal data (requests received and their states and other useful information like timing). Be careful with large contexts due to long-term testing (or load testing) as this is not limited by any configuration maximum (not considered).

You could retrieve a specific entry providing *requestMethod*, *requestUri* and *requestNumber* through query parameters (separator is `'&'`), for example:

`/provision/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&requestNumber=3`

If case that *requestNumber* is omitted, the whole requests history for the *method/uri* provided will be retrieved. Why *h2agent* stores the whole history instead of the last received request for a given key ?: some simulated systems have its own state, so, simplification cannot be assumed: different requests could be received for the same *method/uri*. The *requestNumber* is the history position (**1..N** in chronological order).  To get the latest one provide -1, to retrieve all of them, omit this query parameter.

Both *method*  and *uri*  shall be provided together to select a single entry history (if one is missing and the other is provided, bad request is obtained).

This operation is useful for testing post verification stages (validate content and/or document schema for an specific interface). Remember that you could start the *h2agent* providing a requests schema file to validate incoming receptions through traffic interface, but external validation allows to apply different schemes (although this need depends on the application that you are mocking), and also permits to match the requests content that the agent received.

#### Response status code

**200** (OK), **204** (No Content), **400** (Bad Request).

#### Response body

Json document containing server internal data, '*null*' if nothing cached.

The information collected is:

* `virtualOriginComingFromMethod`: optional, special field for virtual entries coming from provisions which established an *out-state* for a foreign method. This entry is necessary to simulate complexes states but you should ignore from the post-verification point of view. The rest of *json* fields will be kept with the original event information, just in case the history is disabled, to allow tracking the maximum information possible.
* `receptionTimestampMs`: event reception *timestamp*.
* `state`: working/current state for the event.
* `headers`: object containing the list of request headers.
* `body`: object containing the request body.
* `previousSate`: original provision state which managed this request.
* `responseBody`: response which was sent.
* `responseDelayMs`: delay which was processed.
* `responseStatusCode`: status code which was sent.
* `responseHeaders`: object containing the list of response headers which were sent.
* `serverSequence`: current server monotonically increased sequence for every reception. In case of a virtual register (if  it contains the field `virtualOriginComingFromMethod`), this sequence was actually not increased for the server data entry shown, only for the original event which caused this one.

### POST /provision/v1/client-initialize

Initializes the client connection to the target server endpoint.
Not implemented in *Phase 1*, see [Implementation Strategy](#implementation-strategy).

#### Request body schema

`POST` request must comply the following schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "targetAddress": {
      "type": "string"
    },
    "targetPort": {
      "type": "integer"
    },
    "responseSchema": {
      "oneOf": [
        {"type": "object"},
        {"type": "string"}
      ]
    }
  },
  "required": [ "targetAddress", "targetPort" ]
}
```

The json schema provided through `responseSchema` field object, will be used to validate responses received by `h2agent` client endpoint. This schema is optional, so it is possible to accept incoming responses without any kind of contraint for them.

#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### POST /provision/v1/send-message

Sends a message to the server endpoint established in the `client-initialize` operation.
Not implemented in *Phase 1*, see [Implementation Strategy](#implementation-strategy).

#### Request body schema

`POST` request must comply the following schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "requestHeader": {
      "additionalProperties": {
        "type": "string"
       },
       "type": "object"
    },
    "requestMethod": {
      "type": "string",
        "enum": ["POST", "GET", "PUT", "DELETE"]
    },
    "requestUri": {
      "type": "string"
    },
    "requestBody": {
      "type": "object"
    },
    "responseHeaders": {
      "additionalProperties": {
        "type": "string"
       },
       "type": "object"
    },
    "responseCode": {
      "type": "integer"
    },
    "responseBody": {
      "type": "object"
    }
  },
  "required": [ "requestMethod", "requestUri" ]
}
```

#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

## How it is delivered

`h2agent` is delivered in a `helm` chart called `h2agent` (`./helm/h2agent`) so you may integrate it in your regular `helm` chart deployments by just adding a few artifacts.
Take as example the component test chart `ct-h2agent` (`./helm/ct-h2agent`), where the main chart is added as a file requirement but could also be added from helm repository:

## How it integrates in a service

1. Add the project's helm repository with alias `erthelm`:

   ```bash
    helm repo add erthelm https://testillano.github.io/helm
   ```

   TODO: when this project is public, the helm repository will be hosted here and work flows for helm publish on `gh-pages` branch will be moved too (https://testillano.github.io/h2agent).

2. Add one dependency to your `Chart.yaml` file per each service you want to mock with `h2agent` service (use alias when two or more dependencies are included).

   ```yaml
   dependencies:
     - name: h2agent
       version: 1.0.0
       repository: alias:erthelm
       alias: h2server

     - name: h2agent
       version: 1.0.0
       repository: alias:erthelm
       alias: h2server2
   ```

3. Refer to `h2agent` values through the corresponding dependency alias, for example `.Values.h2server.image` to access process repository and tag.

### Agent configuration

At the moment, the server request schema is the only configuration file used by the `h2agent` process and could be added by mean a `config map`. The rest of parameters are passed through [command line](#command-line).

## Implementation strategy

### Phase 1: standalone & memory cached

At a first phase, we will only support one server interface (apart from the administration one), in order to cover http2 server mock features. As future proof, the agent should support any amount of endpoints (clients or servers).
So, the `initialize` operation which provides the server endpoint is not going to be implemented.

The provision information will be stored at process memory, so there is a handycap with possible crashes.

#### Provision

Each time a provision is received, we will create a map key like `method + uri` saving the provision information. When a request is received, the process will search the key composed within the provision map and get the provision content to respond.

Repeated provisions over the same key, although not usual, will overwrite the existing information.

#### Queue system

It could be a case of use that subsequent requests for a given URI could respond differently. For this case, we will have a queue of responses in a given order. This is solved with the `inState` and `outState` mechanism [described](#instate-and-outstate) in this draft.

#### Add metrics

Use [Prometheus](https://prometheus.io/) scrapping system.

[Jun,12 2021] This is the only thing pending in Phase 1

#### Advanced transformations

Add conditional transform filters.

### Phase 2: provide scalability

This is intented to be used to manage high load rates.
In order to have scalability, we will use a shared file system between replicas.

Consider extended attributes to store metadata if the file system support it.

#### Cache system

Each time a provision is received by an specific replica, it will be stored in memory as described in `Phase 1`, and will create a new directory with the path:

`/shared/<base64 of method + uri>/<provision>`

The content (provision) inside that directory will be described later.

The agent service will balance between replicas, so only one of them will cache the provision at the first time, but with the time, the traffic will force to cache the information along the replicas available through shared filesystem reads: when requests are received, the process will search the memory objects getting them from disk in case cache fails, saving the information for the next time.

This cache based in file system is only for internal use, although it could be hacked manually just storing the needed information, for example, by mean a config map. Anyway that's not the purpose of this filesystem information, as the agent offers a REST API to do that.

The cache system will always be enabled (not configurable), and although only one replica could be used with this implementation phase, the cache will improve step by step the processing time as all the provision will be at memory at the end.

#### Queue system

Queued states could be persisted in the file system or extended attributes.

`/shared/<base64 of method + uri>/provision.<queueId>.json`

The base64 encode is used to avoid forbiden characters on filesystem naming.

By default, a unique queue element would be created on provision (queueId = 0), but you could provide any order number: queueId = 1, 2, etc. Every time a request is received, the corresponding provision is searched and evolved in the following way:

* Symlink `provision.json` missing: it will be read, processed and then symlinked to the smallest queue identifier.
* Symlink `provision.json` exists: it will be read, processed and then symlinked to the next queue id.

### Phase 3: multiple endpoints

We will activate the `initialize` administration operation to support N server endpoints as well as client endpoints (although this use case could be not so useful as many test frameworks, like pytest with hyper, provide easy ways to initiate test flows).

This will be probably discarded, as kubernetes allow to separate functionalities on different micro services for a given process.

### Phase 4: Improve scalability design

Consider a key/value database (i.e. [apache geode](https://geode.apache.org/)) instead of file system to cache and queue provision within scalable test system.

## Contributing

You must follow these steps:

### Fork

Fork the project and create a new branch. Check [here](https://chris.beams.io/posts/git-commit/) for a good reference of universal conventions regarding how to describe commit messages and make changes.

### Run unit tests

See [unit test](unit-test).

### Run component tests

See [component test](component-test).

### Check formatting

Please, execute `astyle` formatting (using [frankwolf image](https://hub.docker.com/r/frankwolf/astyle)) before any pull request:

```bash
$ sources=$(find . -name "*.hpp" -o -name "*.cpp")
$ docker run -it --rm -v $PWD:/data frankwolf/astyle ${sources}
```

### Pull request

Rebase to update and then make a `pull request`.

