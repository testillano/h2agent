[TOC]

# C++ HTTP/2 Agent Service - WORK IN PROGRESS

`h2agent` is a network service that enables mocking other network services.

When developing a network service, one often needs to integrate it with other services. However, integrating full-blown versions of such services in a development setup is not always suitable, for instance when they are either heavyweight or not fully developed.

`h2agent` can be used to replace one of those, which allows development to progress and testing to be conducted in isolation against such a service.

`h2agent` supports HTTP2 as a network protocol and JSON as a data interchange language.

## Build project with docker

### Builder image

You shall need the `h2agent_build` docker image to build the project. This image is already available at `docker hub` for every repository `tag` (tagged by mean leading 'v' removal), and also for `master` (tagged as `latest`), so you could skip this step and go directly to the [next](#build-executable), as docker will pull it automatically when needed.

Anyway, builder image could be created using `./docker/h2agent_build/Dockerfile`:

```bash
$ dck_dn=./docker/h2agent_build
$ bargs="--build-arg make_procs=$(grep processor /proc/cpuinfo -c)"
$ docker build --rm ${bargs} -f ${dck_dn}/Dockerfile -t testillano/h2agent_build ${dck_dn}
```

### Build executable

With the previous image, we can now build the executable:

```bash
$ envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         testillano/h2agent_build
```

Environment variables `BUILD_TYPE` (for `cmake`) and `MAKE_PROCS` (for `make`) are inherited from base image (`http2comm_build` -> `nghttp2_build`).

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

`h2agent` listens on a specific management port for incoming requests, implementing an REST API to manage the process operation. Through the API we could program the agent behaviour:

### initialize-server

Not implemented in *Phase 1*, see [Implementation Strategy](#implementation-strategy).

`POST` request must comply the following schema:

```json
{
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
  }
  "required": [ "serverPort" ]
}
```

Initializes the server endpoint for the provided listen port (mandatory).

The json schema provided through `requestSchema` field object, will be used to validate requests received by `h2agent` server endpoint. This schema is optional, so it is possible to accept incoming requests without any kind of contraint for them.

### provision-server

Defines the response behaviour for an incoming request matching some basic conditions (method, uri) and programming the response (header, code, body). The request uri will be interpreted as a regular expression. It is possible to program a response delay in milliseconds.

Also, replace rules can be provided for dynamic response adaptation:

```json
{
  "type": "object",
  "additionalProperties": false,
  "properties": {
    "requestMethod": {
      "type": "string",
        "enum": ["POST", "GET", "PUT", "DELETE"]
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
    "replaceRules": {
      "type": "object"
    },
    "queueId":{
      "type": "integer"
    }
  }
  "required": [ "requestMethod", "requestUri", "responseCode" ]
}
```

#### Replace rules

These rules are used to modify the `responseBody` dynamically in order to adapt provisioned templates for different sources. It is an optional field which will be ignored in case that `responseBody` is not present.

This field is a json object with multiple key/value pairs where keys are reserved for specific purposes:

##### keys `uriCapture` and `bodyCapture`

We could access URI parts by mean providing the position number (1..N) and also negative numbers to back access the path. Also, query parts can be accesed with `query.<parameter name>`.

The URI capture value associated will have the format: `<label>=<capture>`.
So, the label will be used later to replace json paths within the response body:

Imagine the URL `path/to/nirvana/level-1234?param1=22&param2=23`:

```
"uriCapture":"var1=1",              var1 will be 'path'
"uriCapture":"var3=3",              var3 will be 'nirvana'
"uriCapture":"varLast=-1",          varLast will be 'level-1234'
"uriCapture":"param1=query.param1", param1 will be '22'
```

Also, we could capture received requests body nodes by mean a dot separated string notation:

```json
{
  "node1": {
    "node2": "blue"
  }
}
```

```
"bodyCapture":"var1=node1.node2"    var1 will be 'blue'
```

Now, we need to specify the target within answered body to update the captured data:

##### key `bodyUpdate`

The value associated will have the format: `<body path>=<label>`.
The body path points to specific body response path by mean dot separated string, for example:

```
"bodyUpdate":"node1.node2.node3=var1"
```

```json
{
  "node1": {
    "node2": {
      "node3":"green"
    }
  }
}
```

In the example, 'green' will be updated to the value for 'var1'.

### initialize-client

```json
{
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
  }
  "required": [ "targetAddress", "targetPort" ]
}
```

Initializes the client connection to the target server endpoint.

The json schema provided through `responseSchema` field object, will be used to validate responses received by `h2agent` client endpoint. This schema is optional, so it is possible to accept incoming responses without any kind of contraint for them.

### send-message

```json
{
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
  }
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

## Executing h2agent

You may take a look to h2agent command-line by just typing `./h2agent -h|--help`:

TODO

```
here command-line output
```

### Traces and printouts

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

It could be a case of use that subsequent requests for a given URI could respond differently. For this case, we will have a queue of responses in a given order.
Provision operation described had a field named 'queueId', which is an integer natural number (0 by default when not provided via REST API).
The incoming requests will trigger the corresponding sequence in a monotonically increased way until last available. Queue will be automatically rotated until provisions are removed.

### Phase 2: provide scalability

This is intented to be used to manage high load rates.
In order to have scalability, we will use a shared filesystem between replicas.

#### Cache system

Each time a provision is received by an specific replica, it will be stored in memory as described in `Phase 1`, and will create a new directory with the path:

`/shared/<base64 of method + uri>/<provision>`

The content (provision) inside that directory will be described later.

The agent service will balance between replicas, so only one of them will cache the provision at the first time, but with the time, the traffic will force to cache the information along the replicas available through shared filesystem reads: when requests are received, the process will search the memory objects getting them from disk in case cache fails, saving the information for the next time.

This cache based in file system is only for internal use, although it could be hacked manually just storing the needed information, for example, by mean a config map. Anyway that's not the purpose of this filesystem information, as the agent offers a REST API to do that.

The cache system will always be enabled (not configurable), and although only one replica could be used with this implementation phase, the cache will improve step by step the processing time as all the provision will be at memory at the end.

#### Queue system

Queued items will be persisted as:

`/shared/<base64 of method + uri>/provision.<queueId>.json`

The base64 encode is used to avoid forbiden characters on filesystem naming.

By default, a unique queue element would be created on provision (queueId = 0), but you could provide any order number: queueId = 1, 2, etc. Every time a request is received, the corresponding provision is searched and evolved in the following way:

* Symlink `provision.json` missing: it will be read, processed and then symlinked to the smallest queue identifier.
* Symlink `provision.json` exists: it will be read, processed and then symlinked to the next queue id.

### Phase 3: multiple endpoints

We will activate the `initialize` administration operation to support N server endpoints as well as client endpoints (although this use case could be not so useful as many test frameworks, like pytest with hyper, provide easy ways to initiate test flows).

### Phase 4: database cache/queue

Consider key/value database instead of filesystem to cache and queue provision within scalable test system.

# Contributing

You must follow these steps:

## Fork

Fork the project and create a new branch.
Please describe commit messages in imperative form and follow common-sense conventions [here](https://chris.beams.io/posts/git-commit/).

## Run unit tests

TODO

## Run component tests

TODO

## Check formatting

Please, execute `astyle` formatting (using [frankwolf image](https://hub.docker.com/r/frankwolf/astyle)) before any pull request:

```bash
$ sources=$(find . -name "*.hpp" -o -name "*.cpp")
$ docker run -it --rm -v $PWD:/data frankwolf/astyle ${sources}
```

## Pull request

Rebase to update and then make a `pull request`.

