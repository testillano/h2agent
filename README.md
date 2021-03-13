[TOC]

# C++ HTTP/2 Agent Service - WORK IN PROGRESS

`h2agent` is a network service that enables mocking other network services.

When developing a network service, one often needs to integrate it with other services. However, integrating full-blown versions of such services in a development setup is not always suitable, for instance when they are either heavyweight or not fully developed.

`h2agent` can be used to replace one of those, which allows development to progress and testing to be conducted in isolation against such a service.

`h2agent` supports HTTP2 as a network protocol and JSON as a data interchange language.

## Build project with docker

### Builder image

You shall need the `h2agent_build` docker image to build the project. This image is already available at `docker hub` for every repository `tag` (tagged by mean leading 'v' removal), and also for `master` (tagged as `latest`), so you could skip this step and go directly to the [next](#usage), as docker will pull it automatically when needed.

Anyway, builder image could be created using `./docker/h2agent_build/Dockerfile`:

```bash
$ dck_dn=./docker/h2agent_build
$ bargs="--build-arg make_procs=$(grep processor /proc/cpuinfo -c)"
$ docker build --rm ${bargs} -f ${dck_dn}/Dockerfile -t testillano/h2agent_build ${dck_dn}
```

### Usage

With the previous image, we can now build the executable:

```bash
$ envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         testillano/h2agent_build
```

Environment variables `BUILD_TYPE` (for `cmake`) and `MAKE_PROCS` (for `make`) are inherited from base image (`http2comm_build` -> `nghttp2_build`):
You could generate documentation understanding the builder script behind ([nghttp2 build entrypoint](https://github.com/testillano/nghttp2_build/blob/master/deps/build.sh)):

```bash
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         testillano/h2agent_build "" doc
```

You could use the script `./build_all.sh` to automate all those operations.

## Docker-executable image

To build an static-autonomous executable docker image, you shall need to build the project docker image, which is located at project root:

```bash
$ bargs+=" --build-arg build_type=Release" # make_procs & build_type
$ docker build --rm ${bargs} -t testillano/h2agent .
```

Again, this image is uploaded for every `tag` and also for `master` branch, to be available for corresponding `helm charts` (normally packaged into releases) which will be described in following [sections](#how-it-is-delivered).

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

Check the requirements described at building `dockerfile` (`./docker/h2agent_build/Dockerfile`) as well as all the ascendant docker images which are inherited:

```
h2agent_build (./docker/h2agent_build/Dockerfile)
   |
http2comm_build (https://github.com/testillano/http2comm_build)
   |
nghttp2_build (https://github.com/testillano/nghttp2_build)
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

TODO

### Unit test

TODO

### Component test

TODO

## How it works

### Executing h2agent

#### Command line

You may take a look to `h2agent` command line by just typing `./h2agent -h|--help`:

```
Usage: h2agent [options]                                                                                                                                             [3/7755]

Options:

[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]
  Set the logging level; defaults to warning.

[--verbose]
  Output log traces on console.

[-a|--admin-port <port>]
  Admin <port>; defaults to 8074

[-p|--server-port <port>]
  Server <port>; defaults to 8000

[-m|--server-api-name <name>]
  Server API name; defaults to empty

[-n|--server-api-version <version>]
  Server API version; defaults to empty

[-w|--max-worker-threads <threads>]
  Maximum worker threads; defaults to -1 (no limit)

[-t|--server-threads <threads>]
  Number of nghttp2 server threads; defaults to 1 (1 connection)

[-k|--server-key <path file>]
  Path file for server key to enable SSL/TLS; defaults to empty

[-c|--server-crt <path file>]
  Path file for server crt to enable SSL/TLS; defaults to empty

[--server-request-schema <path file>]
  Path file for the server schema to validate requests received

[-v|--version]
  Program version

[-h|--help]
  This help

```

#### Traces and printouts

Traces are managed by `syslog` by default, but could be shown verbosely at standard output (`--verbose`) depending on the traces design level and the current level assigned:

```
<todo>
```

### Management interface

`h2agent` listens on a specific management port (*8074* by default) for incoming requests, implementing a *REST API* to manage the process operation. Through the *API* we could program the agent behavior. The following subsections **name** the operations which would be commanded by a *POST* request with *URI* `provision/v1/<operation>`. The general procedure is to retrieve the corresponding provision which stores information of "how to answer" the reception.

**Current development phase is 1**, see [Implementation Strategy](#implementation-strategy).

#### server-initialize

Not implemented in *Phase 1*, see [Implementation Strategy](#implementation-strategy).

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

Initializes the server endpoint for the provided listen port (mandatory).

The `json` schema provided through `requestSchema` field object, will be used to validate requests received by `h2agent` server endpoint (you could constraint specific values with `"const"` from `json` schema [draft 6](http://json-schema.org/draft/2019-09/json-schema-validation.html#rfc.section.6.1.3)). This schema is optional, so it is possible to accept incoming requests without any kind of restriction for them.

During *Phase 1* the request schema is passed through [command line](#command-line) by mean `--server-request-schema` option.

#### server-matching

Defines the server matching procedure for incoming receptions on mock service. Every *URI* received is matched depending on the selected algorithm. This is the operation request body schema:

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
    "sortUriPathQueryParameters": {
      "type": "boolean"
    }
  }
}
```

##### sortUriPathQueryParameters

Optional boolean argument used to sort query parameters received in the *URI* path. It is `true` by default to ensure a kind of normalization which makes things more predictable from provision point of view.

##### rgx & fmt

Optional arguments used in `FullMatchingRegexReplace` algorithm.

##### algorithm

###### FullMatching

No additional arguments are expected. The incoming request is fully translated into key without any manipulation, and then searched in internal provision map.

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

No additional arguments are required. This identification algorithm relies in the original provision order to match the receptions and reach the first valid occurrence. For example, consider 3 provision operations which are provided sequentially in the following order:

1. `ctrl/v2/id-55500[0-9]{4}/ts-[0-9]{10}`
2. `ctrl/v2/id-5551122[0-9]{2}/ts-[0-9]{10}`
3. `ctrl/v2/id-555112244/ts-[0-9]{10}`

If the `URI` "*ctrl/v2/id-555112244/ts-1615562841*" is received, the second one is the first positive match and then, selected to mock the provisioned answer. Even being the third one more accurate, this algorithm establish an ordered priority to match the information.

#### server-provision

Defines the response behavior for an incoming request matching some basic conditions (*method*, *uri*) and programming the response (*header*, *code*, *body*).

This is the body request schema supported:

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
        {"required": ["ToUpper"]},
        {"required": ["ToLower"]},
        {"required": ["Padded"]}
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
        "ToUpper": { "type": "string" },
        "ToLower": { "type": "string" },
        "Padded": { "type": "integer" }
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
        "enum": ["POST", "GET", "PUT", "DELETE" ]
    },
    "requestUri": {
      "type": "string"
    },
    "responseHeader": {
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
        "properties": {
          "source": {
            "type": "string",
            "pattern": "^var\\..+|^request\\.uri$|^request\\.uri\\.path$|^request\\.uri\\.param\\..+|^request\\.body$|^request\\.body\\..+|^request\\.header\\..+|^general\\.random\\..+|^general\\.timestamp\\.ns$|^general\\.unique$|^inState$"
          },
          "target": {
            "type": "string",
            "pattern": "^var\\..+|^response\\.body$|^response\\.body\\..+|^response\\.header\\..+|^response\\.statusCode$|^outState$"
          }
        },
        "additionalProperties" : {
          "$ref" : "#/definitions/filter"
        },
        "required": [ "source", "target" ]
      }
    }
  },
  "required": [ "requestMethod", "requestUri", "responseCode" ]
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

Expected request method (*POST*, *GET*, *PUT*, *DELETE*).

##### requestUri

Request *URI* path to match depending on the algorithm selected.

##### responseHeader

Header fields for the response.

##### responseCode

Response status code.

##### responseBody

Response body.

##### responseDelayMs

Optional response delay simulation in milliseconds.

##### transform

Sorted list of transformations to modify incoming information and build the dynamic response to be sent.

Each transformation has a `source`, a `target` and an optional `filter` algorithm.

The source of information is always a string representation, and could be one of the following:

- request.uri: whole request *URI*  path, including the possible query parameters.

- request.uri.path: request *URI* path part.

- request.uri.param.<name>: request URI specific parameter `<name>`.

- request.body: request body document.

- request.body.<node1>..<nodeN>: request body node path.

- request.header.<hname>: request header component (i.e. *content-type*).

- general.random.<range>: integer number up to `<range>` value.

- general.timestamp.ns: UNIX epoch time in nanoseconds.

- general.unique: sequence id increased for every mock reception.

- var.<id>: general purpose variable.

- inState: current processing state.



The target of information is always a string representation, and could be one of the following:

- response.body: response body document.
- response.body.<node1>..<nodeN>: response body node path.
- response.header.<hname>: response header component (i.e. *location*).
- response.statusCode: response status code.
- var.<id>: general purpose variable.
- outState: next processing state. This overrides the default provisioned one.



There are several filter methods:



- RegexCapture: this filter provides a regular expression with capture groups which will be applied to the source and stored in the target with the keys = `<target>.N`, being *N* each captured group. This filter only work with general purpose variables as target. For example:

```json
{
  "source": "request.uri.path",
  "target": "var.id_cat",
  "filter": { "RegexCapture" : "api\/v2\/id-([0-9]+)\/category-([a-z]+)" }
}
```

For example, if the source received is *"api/v2/id-28/category-animal/"*, then we have 2 captured groups, so, we will have: *var.id_cat.1="28"* and *var.id_cat.2="animal"*.



- RegexReplace: this is similar to the matching algorithm based in regular expressions and replace procedure. We provide `rgx` and `fmt` to transform the source into the target:

```json
{
  "source": "request.uri.path",
  "target": "response.body.data.timestamp",
  "filter": {
    "RegexReplace" : {
      "rgx" : "(ctrl/v2/id-[0-9]+/)ts-([0-9]+)",
      "fmt" : "$2"
    }
  }
}
```

For example, if the source received is "*ctrl/v2/id-555112233/ts-1615562841*", then we will replace/create a node "*data.timestamp*" within the response body, with the value formatted: *1615562841*.

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
  "source": "var.subdomain",
  "target": "var.site",
  "filter": { "Append" : ".teslayout.com" }
}
```

For example, if the source received is "*telegram*", then we will will have *var.site="telegram.teslayout.com"*.

This could be done also with the `RegexReplace` filter, but this has better performance.



- ToUpper: transforms the source into upper case.



- ToLower: transforms the source into lower case.



- Padded: transforms the source with leading (positive input) or trailing (negative input) zeros to complete the provided absolute value as the whole string size.

```json
{
  "source": "var.sequence",
  "target": "var.paddedSequence",
  "filter": { "Padded" : 9 }
}
```

For example, if the source received is "55511", then we will will have *var.paddedSequence="555110000"*. In case that the value provided is negative (*-9*), then this would be the result: "*000055511*".



#### client-initialize

Not implemented in *Phase 1*, see [Implementation Strategy](#implementation-strategy).

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

Initializes the client connection to the target server endpoint.

The json schema provided through `responseSchema` field object, will be used to validate responses received by `h2agent` client endpoint. This schema is optional, so it is possible to accept incoming responses without any kind of contraint for them.

#### send-message

Not implemented in *Phase 1*, see [Implementation Strategy](#implementation-strategy).

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
    "responseHeader": {
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

## How it is delivered

`h2agent` is delivered in a Helm chart called `h2agent` (`./helm/h2agent`) so you may integrate it in your regular helm chart deployments by just adding a few artifacts.
Take as example the component test chart `ct-h2agent` (`./helm/ct-h2agent`), where the main chart is added as a requirement:

## How it integrates in a service

1. Make sure that the "erthelm" Helm repository (where the `h2agent` helm chart is stored) is defined with `helm repo list`:

   TODO

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

3. Reference `h2agent` in your chart under `.Values.h2agent.image` repository and tag.

4. Define a config map for the agent configuration and reference it in your values with the name you gave it under `.Values.h2agent.config.cm`.

### Agent configuration

The config map mentioned above contains the agent configuration. It must be in json format, and compliant with its schema definition. You may ask for the schema just by executing `./h2agent --conf-schema`.

#### Schema definition

TODO

#### Example

TODO

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

