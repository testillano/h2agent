# C++ HTTP/2 Mock Service

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://codedocs.xyz/testillano/h2agent.svg)](https://codedocs.xyz/testillano/h2agent/index.html)
[![API](https://img.shields.io/badge/API-OpenAPI%203.1-6ba539.svg)](https://testillano.github.io/h2agent/api/)
[![Coverage Status](https://img.shields.io/endpoint?url=https://testillano.github.io/h2agent/coverage/badge.json)](https://testillano.github.io/h2agent/coverage)
[![Ask Me Anything !](https://img.shields.io/badge/Ask%20me-anything-1abc9c.svg)](https://github.com/testillano)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/testillano/h2agent/graphs/commit-activity)
[![CI](https://github.com/testillano/h2agent/actions/workflows/ci.yml/badge.svg)](https://github.com/testillano/h2agent/actions/workflows/ci.yml)
[![Docker Pulls](https://img.shields.io/docker/pulls/testillano/h2agent.svg)](https://github.com/testillano/h2agent/pkgs/container/h2agent)

`H2agent` is a network service agent that enables **mocking HTTP/2 applications** (also HTTP/1.x is supported via `nghttpx` reverse proxy).
It is mainly designed for testing, but could even simulate or complement advanced services.

It is being used intensively in E/// company, as part of testing environment for some telecommunication products.



As a brief **summary**, we could <u>highlight the following features</u>:

* Mock types:

  * Server (unique)
  * Client (multiple clients may be provisioned)

* Testing

  * Functional/Component tests:
  * System tests (KPI, High Load, Robustness).
  * Congestion Control.
  * Validations:
    * Optionally tied to provision with machine states.
    * Sequence validation can be decoupled (order by user preference).
    * Available traffic history inspection (REST API).

* Traffic protocols

  * HTTP/1/2.
  * UDP.

* TLS/SSL support

  * Server
  * Client

* Interfaces

  * Administrative interface (REST API): update, create and delete items:
    * Global variables.
    * File manager configuration.
    * UDP events.
    * Schemas (Rx/Tx).
    * Logging system.
    * Traffic classification and provisioning configuration.
    * Client endpoints configuration.
    * Events data (server and clients) summary and inspection.
    * Events data configuration (global storage, history).
  * Prometheus metrics (HTTP/1):
    * Counters by method and result.
    * Gauges and Histograms (response delays, message size for Rx/Tx).
  * Log system (POSIX Levels).
  * Command line:
    * Administrative & traffic interfaces (addresses, ports, security certificates).
    * Congestion control parameters (workers, maximum workers and maximum queue size).
    * Schema configuration json documents to be referred (traffic validation).
    * Global variables json document.
    * File manager configuration.
    * Traffic server configuration (classification and provision).
    * Metrics interface: histogram buckets for server and client and for histogram types.
    * Clients connection behaviour (lazy/active).

* Schema validation

  * Administrative interface.
  * Mock system (Tx/Rx requests).

* Traffic classification

  * Full matching.
  * Regular expression matching.
  * Priority regular expression matching.
  * Query parameters filtering (sort/pass by/ignore).
  * Query parameters delimiters (ampersand/semicolon)

* Programming:

  * User-defined machine state (FSM).
  * Internal events system (data extraction from past events).
  * Global variables.
  * File system operations.
  * UDP writing operations.
  * Response build (headers, body, status code, delay).
  * Transformation algorithms: thousands of combinations
    * Sources: uri, uri path, query parameters, bodies, request/responses bodies and paths, headers, eraser, math expressions, shell commands, random generation (ranges, sets), unix timestamps, strftime formats, sequences, dynamic variables, global variables, constant values, input state (working state), events, files (read).
    * Filters: regular expression captures and regex/replace, append, prepend, basic arithmetics (sum, multiply), equality, condition variables, differences, json constraints and schema id.
    * Targets: dynamic variables, global variables, files (write), response body (as string, integer, unsigned, float, boolean, object and object from json string), UDP through unix socket (write), response body path (as string, integer, unsigned, float, boolean, object and object from json string), headers, status code, response delay, output state, events, break conditions.
  * Multipart support.
  * Pseudo-notification mechanism (response delayed by global variable condition).

* Training:

  * Questions and answers for project documentation using **openai** (ChatGPT-based).
  * Playground.
  * Demo.
  * Tester.
  * Kata exercises.

* Tools programs:

  * Matching helper.
  * Arash Partow helper (math expressions).
  * HTTP/2 client.
  * UDP server.
  * UDP server to trigger active HTTP/2 client requests.
  * UDP client.

---

<details>
<summary>Table of Contents</summary>

- [Quick start](#quick-start)
- [Scope](#scope)
- [How can you use it ?](#how-can-you-use-it-)
- [Static linking](#static-linking)
- [Project image](#project-image)
- [Build project with docker](#build-project-with-docker)
- [Build project natively](#build-project-natively)
- [Testing](#testing)
- [Execution of main agent](#execution-of-main-agent)
- [Execution of matching helper utility](#execution-of-matching-helper-utility)
- [Execution of Arash Partow's helper utility](#execution-of-arash-partows-helper-utility)
- [Execution of h2client utility](#execution-of-h2client-utility)
- [UDP utilities](#udp-utilities)
  - [Execution of udp-server utility](#execution-of-udp-server-utility)
  - [Execution of udp-server-h2client utility](#execution-of-udp-server-h2client-utility)
  - [Execution of udp-client utility](#execution-of-udp-client-utility)
- [Working with unix sockets and docker containers](#working-with-unix-sockets-and-docker-containers)
- [Execution with TLS support](#execution-with-tls-support)
- [Metrics](#metrics)
- [Traces and printouts](#traces-and-printouts)
- [Training](#training)
- [Management interface](#management-interface)
- [Dynamic response delays](#dynamic-response-delays)
- [Reserved Global Variables](#reserved-global-variables)
- [How it is delivered](#how-it-is-delivered)
- [How it integrates in a service](#how-it-integrates-in-a-service)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

</details>

---

## Quick start

**Theory**

* A ***[prezi](https://prezi.com/view/RFaiKzv6K6GGoFq3tpui/)*** presentation to show a complete and useful overview of the `h2agent` component architecture.
* A conversational bot for [***questions & answers***](./README.md#questions-and-answers) based in *Open AI*.

**Practice**

* Brief exercises to ***[play](./README.md#Play)*** with, showing basic configuration "games" to have a quick overview of project possibilities.
* Tester GUI tool to ***[test](./README.md#Test)*** with, allowing to make quick interactions through traffic and administrative interfaces.
* A ***[demo](./README.md#Demo)*** exercise which presents a basic use case to better understand the project essentials.
* And finally, a ***[kata](./README.md#Kata)*** training to acquire better knowledge of project capabilities.

Bullet list of exercises above, have a growing demand in terms of attention and dedicated time. For that reason, they are presented in the indicated order, facilitating and prioritizing simplicity for the user in the training process.

## Scope

When developing a network service, one often needs to integrate it with other services. However, integrating full-blown versions of such services in a development setup is not always suitable, for instance when they are either heavyweight or not fully developed.

`H2agent` can be used to replace one (or many) of those, which allows development to progress and testing to be conducted in isolation against such a service.

`H2agent` supports HTTP/2 as a network protocol (also HTTP/1.x via proxy) and JSON as a data interchange language.

So, `h2agent` could be used as:

* **Server** mock: fully implemented.
* **Client** mock: fully implemented.

Also, `h2agent` can be configured through **command-line** but also dynamically through an **administrative HTTP/2 interface** (`REST API`). This last feature makes the process a key element within an ecosystem of remotely controlled agents, enabling a reliable and powerful orchestration system to develop all kinds of functional, load and integration tests. So, in summary `h2agent` offers two execution planes:

* **Traffic plane**: application flows.
* **Control plane**: traffic flow orchestration, mocks behavior control and SUT surroundings monitoring and inspection.

Check the [releases](https://github.com/testillano/h2agent/releases) to get latest packages, or read the following sections to build all the artifacts needed to start playing:

## How can you use it ?

`H2agent` process (as well as other project binaries) may be used natively, as a `docker` container, or as part of `kubernetes` deployment.

The easiest way to build the project is using [containers](https://en.wikipedia.org/wiki/LXC) technology (this project uses `docker`): **to generate all the artifacts**, just type the following:

```bash
$ ./build.sh --auto
```

The option `--auto` builds the <u>builder image</u> (`--builder-image`) , then the <u>project image</u> (`--project-image`) and finally <u>project executables</u> (`--project`). Then you will have everything available to run binaries with different modes:

* Run <u>h2agent project image</u> with docker (`./run.sh` script at root directory can also be used):

  ```bash
  $ docker run --rm -it -p 8000:8000 -p 8074:8074 -p 8080:8080 ghcr.io/testillano/h2agent:latest # default entrypoint is h2agent process
  ```

  Exported ports correspond to server defaults: traffic(8000), administrative(8074) and metrics(8080), but of course you could configure your own externals.
  You may override default entrypoint (`/opt/h2agent`) to run another binary packaged (check project `Dockerfile`), for example the simple client utility:

  ```bash
  $ docker run --rm -it --network=host --entrypoint "/opt/h2client" ghcr.io/testillano/h2agent:latest --uri http://localhost:8000/unprovisioned # run in another shell to get response from h2agent server launched above
  ```

  Or any other packaged utility (if you want to lighten the image size, write your own Dockerfile and get what you need):

  ```bash
  $ docker run --rm -it --network=host --entrypoint "/opt/matching-helper" ghcr.io/testillano/h2agent:latest --help
  -or-
  $ docker run --rm -it --network=host --entrypoint "/opt/arashpartow-helper" ghcr.io/testillano/h2agent:latest --help
  -or-
  $ docker run --rm -it --network=host --entrypoint "/opt/h2client" ghcr.io/testillano/h2agent:latest --help
  -or-
  $ docker run --rm -it --network=host --entrypoint "/opt/udp-server" ghcr.io/testillano/h2agent:latest --help
  -or-
  $ docker run --rm -it --network=host --entrypoint "/opt/udp-server-h2client" ghcr.io/testillano/h2agent:latest --help
  -or-
  $ docker run --rm -it --network=host --entrypoint "/opt/udp-client" ghcr.io/testillano/h2agent:latest --help
  ```

* HTTP/x proxy supporting HTTP/1.0, HTTP/1.1, HTTP/2 and HTTP/2 without HTTP/1.1 Upgrade (prior knowledge as `h2agent` provides), with docker (`./run.sh` script at root directory can also be used with some prepends):

  This proxy, encapsulated within the Docker image, is latent until activated by configuration:
  Proxy front-end ports are configured though environment variables: `H2AGENT_TRAFFIC_PROXY_PORT` (for traffic) and `H2AGENT_ADMIN_PROXY_PORT` (for administration). Proxy back-end ports are also configured, for traffic and administrative interface, by mean `H2AGENT_TRAFFIC_SERVER_PORT` (8000 by default) and `H2AGENT_ADMIN_SERVER_PORT` (8074 by default). Traffic frontend handles requests to the backend port exposed by the `h2agent` traffic server. Likewise, administrative requests are forwarded to the backend port of the `h2agent` administrative server.

  The proxy feature complements `nghttp2 tatsuhiro library` which only provides HTTP/2 protocol without upgrade support from HTTP/1).

  Note that:
  `H2AGENT_TRAFFIC_SERVER_PORT` **must be aligned with `--traffic-server-port` h2agent parameter**, or `502 Bad Gateway` error will be obtained.
  `H2AGENT_ADMIN_SERVER_PORT` **must be aligned with `--admin-server-port` h2agent parameter**, or `502 Bad Gateway` error will be obtained.

  Example with proxy enabled for traffic interface (same applies to enable administrative interface, or both of them):

  ```bash
  $ docker run --rm -it -p 8555:8001 -p 8000:8000 -p 8074:8074 -p 8080:8080 -e H2AGENT_TRAFFIC_PROXY_PORT=8001 ghcr.io/testillano/h2agent:latest &

  $ curl -i http://localhost:8555/arbitrary/path # through proxy (same using --http1.0 or --http1.1)
  HTTP/1.1 501 Not Implemented
  Date: <date>
  Transfer-Encoding: chunked
  Via: 2 nghttpx

  $ curl -i --http2 http://localhost:8555/arbitrary/path # through proxy
  HTTP/1.1 101 Switching Protocols
  Connection: Upgrade
  Upgrade: h2c

  HTTP/2 501
  date: <date>
  via: 2 nghttpx

  $ curl -i --http2-prior-knowledge http://localhost:8555/arbitrary/path # through proxy
  HTTP/2 501
  date: <date>
  via: 2 nghttpx

  $ curl -i --http2-prior-knowledge http://localhost:8000/arbitrary/path # directly to h2agent
  HTTP/2 501
  date: <date>
  ```

  This mode is also useful to play with `nginx` balancing capabilities (check this [gist](https://gist.github.com/testillano/3f7ff732850f42a6e7ee625aa182e617)).

* Run within `kubernetes` deployment: corresponding `helm charts` are normally packaged into releases. This is described in ["how it is delivered"](#How-it-is-delivered) section, but in summary, you could do the following:

  ```bash
  $ # helm dependency update helm/h2agent # no dependencies at the moment
  $ helm install h2agent-example helm/h2agent --wait
  $ pod=$(kubectl get pod -l app.kubernetes.io/name=h2agent --no-headers -o name)
  $ kubectl exec ${pod} -c h2agent -- /opt/h2agent --help # run, for example, h2agent help
  ```

  You may enter the pod and play with helpers functions and examples (deployed with the chart under `/opt/utils`) which are anyway, automatically sourced on `bash` shell:

  ```bash
  $ kubectl exec -it ${pod} -- bash
  ```

It is also possible to build the project natively (not using containers) installing all the dependencies on the local host:

```bash
$ ./build-native.sh # you may prepend non-empty DEBUG variable value in order to troubleshoot build procedure
```

So, you could run `h2agent` (or any other binary available under `build/<build type>/bin`) directly:


* Run <u>project executable</u> natively (standalone):

  ```bash
  $ build/Release/bin/h2agent & # default server at 0.0.0.0 with traffic/admin/prometheus ports: 8000/8074/8080
  ```

  Provide `-h` or `--help` to get **process help** (more information [here](#Execution-of-main-agent)) or execute any other project executable.

  You may also play with project helpers functions and examples:

  ```bash
  $ source tools/helpers.bash # type help in any moment after sourcing
  $ server_example # follow instructions or just source it: source <(server_example)
  $ client_example # follow instructions or just source it: source <(client_example)
  ```


## Static linking

Both build helpers (`build.sh` and `build-native.sh` scripts) allow to force project static link, although this is [not recommended](https://stackoverflow.com/questions/57476533/why-is-statically-linking-glibc-discouraged):

```bash
$ STATIC_LINKING=TRUE ./build.sh --auto
- or -
$ STATIC_LINKING=TRUE ./build-native.sh
```

So, you could run binaries regardless if needed libraries are available or not (including `glibc` with all its drawbacks).




Next sections will describe in detail, how to build [project image](#Project-image) and project executables ([using docker](#Build-project-with-docker) or [natively](#Build-project-natively)).

## Project image

This image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull ghcr.io/testillano/h2agent:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$ ./build.sh --project-image
```

This image is built with `./Dockerfile`.
Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.
If you want to work with alpine-based images, you may build everything from scratch, including all docker base images which are project dependencies.

## Build project with docker

### Builder image

This image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull ghcr.io/testillano/h2agent_builder:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$ ./build.sh --builder-image
```

This image is built with `./Dockerfile.build`.
Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.
If you want to work with alpine-based images, you may build everything from scratch, including all docker base images which are project dependencies.

### Usage

Builder image is used to build the project. To run compilation over this image, again, just run with `docker`:

```bash
$ envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
          ghcr.io/testillano/h2agent_builder:<tag>
```

You could generate documentation passing extra arguments to the [entry point](https://github.com/testillano/nghttp2/blob/master/deps/build.sh) behind:

```bash
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
          ghcr.io/testillano/h2agent_builder::<tag> "" doc
```

You could also build the library using the script `./build.sh` located at project root:


```bash
$ ./build.sh --project
```

## Build project natively

It may be hard to collect every dependency, so there is a native build **automation script**:

```bash
$ ./build-native.sh
```

Note 1: this script is tested on `ubuntu bionic`, then some requirements could be not fulfilled in other distributions.

Note 2: once dependencies have been installed, you may just type `cmake . && make` to have incremental native builds.

Note 3: if not stated otherwise, this document assumes that binaries (used on examples) are natively built.



Anyway, we will describe the common steps for a `cmake-based` building project like this. Firstly you may install `cmake`:

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
$ cmake -DMY_OWN_INSTALL_PREFIX=$HOME/applications/http2
$ make install
```

### Uninstall

```bash
$ cat install_manifest.txt | sudo xargs rm
```

## Testing

### Unit test

Check the badge above to know the current coverage level.
You can execute it after project building, for example for `Release` target:

```bash
$ build/Release/bin/unit-test # native executable
- or -
$ docker run -it --rm -v ${PWD}/build/Release/bin/unit-test:/ut --entrypoint "/ut" ghcr.io/testillano/h2agent:latest # docker
```

To shortcut docker run execution, `./ut.sh` script at root directory can also be used.
You may provide extra arguments to Google test executable, for example:

```bash
$ ./ut.sh --gtest_list_tests # to list the available tests
$ ./ut.sh --gtest_filter=Transform_test.ProvisionWithResponseBodyAsString # to filter and run 1 specific test
$ ./ut.sh --gtest_filter=Transform_test.* # to filter and run 1 specific suite
etc.
```

#### Coverage

Coverage reports can be generated using `./tools/coverage.sh`:

```bash
./tools/coverage.sh [ut|ct|all]
```

- `ut`: Unit test coverage only (default for CI)
- `ct`: Component test coverage only (requires Kubernetes cluster)
- `all`: Combined UT + CT coverage (default)

Reports are generated in:
- `coverage/ut/` - Unit test coverage
- `coverage/ct/` - Component test coverage
- `coverage/combined/` - Combined coverage

The script builds Docker images from `Dockerfile.coverage.ut` and `Dockerfile.coverage.ct`, using `lcov` for instrumentation. A `firefox` instance is launched to display the report.

Both `ubuntu` and `alpine` base images are supported.

### Component test

Component test is based in `pytest` framework. Just execute `ct/test.sh` to deploy the component test chart. Some cloud-native technologies are required: `docker`, `kubectl`, `minikube` and `helm`, for example:

```bash
$ docker version
Client: Docker Engine - Community
 Version:           20.10.17
 API version:       1.41
 Go version:        go1.17.11
 Git commit:        100c701
 Built:             Mon Jun  6 23:02:56 2022
 OS/Arch:           linux/amd64
 Context:           default
 Experimental:      true

Server: Docker Engine - Community
 Engine:
  Version:          20.10.17
  API version:      1.41 (minimum version 1.12)
  Go version:       go1.17.11
  Git commit:       a89b842
  Built:            Mon Jun  6 23:01:02 2022
  OS/Arch:          linux/amd64
  Experimental:     false
 containerd:
  Version:          1.6.6
  GitCommit:        10c12954828e7c7c9b6e0ea9b0c02b01407d3ae1
 runc:
  Version:          1.1.2
  GitCommit:        v1.1.2-0-ga916309
 docker-init:
  Version:          0.19.0
  GitCommit:        de40ad0

$ kubectl version
Client Version: version.Info{Major:"1", Minor:"22", GitVersion:"v1.22.4", GitCommit:"b695d79d4f967c403a96986f1750a35eb75e75f1", GitTreeState:"clean", BuildDate:"2021-11-17T15:48:33Z", GoVersion:"go1.16.10", Compiler:"gc", Platform:"linux/amd64"}
Server Version: version.Info{Major:"1", Minor:"22", GitVersion:"v1.22.2", GitCommit:"8b5a19147530eaac9476b0ab82980b4088bbc1b2", GitTreeState:"clean", BuildDate:"2021-09-15T21:32:41Z", GoVersion:"go1.16.8", Compiler:"gc", Platform:"linux/amd64"}

$ minikube version
minikube version: v1.23.2
commit: 0a0ad764652082477c00d51d2475284b5d39ceed

$ helm version
version.BuildInfo{Version:"v3.7.1", GitCommit:"1d11fcb5d3f3bf00dbe6fe31b8412839a96b3dc4", GitTreeState:"clean", GoVersion:"go1.16.9"}
```

### Benchmarking test

This test is useful to identify possible memory leaks, process crashes or performance degradation introduced with new fixes or features.

Reference:

* VirtualBox VM with Linux Bionic (Ubuntu 18.04.3 LTS).

* Running on Intel(R) Core(TM) i7-8650U CPU @1.90GHz.

* Memory size: 15GiB.



Load testing is done with [h2load](https://nghttp2.org/documentation/h2load-howto.html), [hermes](https://github.com/jgomezselles/hermes) and `h2agent` itself (client mode) using the helper script `benchmark/start.sh` (check `-h|--help` for more information). The available launchers are:

* **h2load**: uses the `nghttp2` load testing tool. Requires `h2load` in `PATH`.
* **hermes**: uses the [hermes](https://github.com/jgomezselles/hermes) Docker image. Requires Docker.
* **h2client**: starts a second `h2agent` instance in client-only mode (`--traffic-server-port -1`) on a separate admin port (default `8075`), configures a client endpoint pointing to the server `h2agent`, and drives load via the timer-based client provision trigger (`rps` + `sequenceEnd`). Polls `client-data` to track progress and reports actual throughput. Requires `h2agent` in `PATH`.

  The client uses a single `boost::asio::steady_timer` that fires one request per tick at `1.000.000/rps` microsecond intervals. The timer is accurate up to at least **30.000 req/s** (~-3% error, dominated by measurement overhead). The default is `rps=10000`. For higher loads, use `h2load` (concurrent streams, no timer overhead).

  Example (non-interactive):
  ```bash
  $ ST_LAUNCHER=h2client H2CLIENT__RPS=10000 H2CLIENT__ITERATIONS=100000 benchmark/start.sh -y
  ```

Also, `benchmark/repeat.sh` script repeats a previous execution (last by default) in headless mode.

#### Considerations

* As schema validation is normally used only for function tests, it will be disabled here.
* `h2agent` could be for example started with 5 worker threads to discard application bottlenecks.
* Add histogram boundaries to better classify internal answer latencies for [metrics](#OAM).
* Data storage is disabled in the script by default to prevent memory from growing and improve server response times (remember that storage shall be kept when provisions require data persistence).
* In general, even with high traffic rates, you could get sneaky snapshots just enabling and then quickly disabling data storage, for example using [function helpers](#Helper-functions): `server_data_configuration --keep-all && server_data_configuration --discard-all`



So you may start the process, again, natively or using docker:

```bash
$ OPTS=(--verbose --prometheus-response-delay-seconds-histogram-boundaries "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3")
$ build/Release/bin/h2agent "${OPTS[@]}" # native executable
- or -
$ docker run --rm -it --network=host -v $(pwd -P):$(pwd -P) ghcr.io/testillano/h2agent:latest "${OPTS[@]}" # docker
- or -
$ XTRA_ARGS="-v $(pwd -P):$(pwd -P)" ./run.sh # benchmark options are provided within run.sh script
```

In other shell we launch the benchmark test:

```bash
$ benchmark/start.sh -y


Input Validate schemas (y|n)
 (or set 'H2AGENT_VALIDATE_SCHEMAS' to be non-interactive) [n]:
n

Input Matching configuration
 (or set 'H2AGENT_SERVER_MATCHING' to be non-interactive) [server-matching.json]:
server-matching.json

Input Provision configuration
 (or set 'H2AGENT_SERVER_PROVISION' to be non-interactive) [server-provision.json]:
server-provision.json

Input Global variable(s) configuration
 (or set 'H2AGENT_GLOBAL_VARIABLE' to be non-interactive) [global-variable.json]:
global-variable.json

Input File manager configuration to enable read cache (true|false)
 (or set 'H2AGENT__FILE_MANAGER_ENABLE_READ_CACHE_CONFIGURATION' to be non-interactive) [true]:
true

Input Server configuration to ignore request body (true|false)
 (or set 'H2AGENT__SERVER_TRAFFIC_IGNORE_REQUEST_BODY_CONFIGURATION' to be non-interactive) [false]:
false

Input Server configuration to perform dynamic request body allocation (true|false)
 (or set 'H2AGENT__SERVER_TRAFFIC_DYNAMIC_REQUEST_BODY_ALLOCATION_CONFIGURATION' to be non-interactive) [false]:
false

Input Server data storage configuration (discard-all|discard-history|keep-all)
 (or set 'H2AGENT__DATA_STORAGE_CONFIGURATION' to be non-interactive) [discard-all]:
discard-all

Input Server data purge configuration (enable-purge|disable-purge)
 (or set 'H2AGENT__DATA_PURGE_CONFIGURATION' to be non-interactive) [disable-purge]:
disable-purge

Input H2agent endpoint address
 (or set 'H2AGENT__BIND_ADDRESS' to be non-interactive) [0.0.0.0]:
0.0.0.0

Input H2agent response delay in milliseconds
 (or set 'H2AGENT__RESPONSE_DELAY_MS' to be non-interactive) [0]:
0

Input Request method (PUT|DELETE|HEAD|POST|GET)
 (or set 'ST_REQUEST_METHOD' to be non-interactive) [POST]:
POST

POST request body defaults to:
   {"id":"1a8b8863","name":"Ada Lovelace","email":"ada@geemail.com","bio":"First programmer. No big deal.","age":198,"avatar":"http://en.wikipedia.org/wiki/File:Ada_lovelace.jpg"}

To override this content from shell, paste the following snippet:

# Define helper function:
random_request() {
   echo "Input desired size in bytes [3000]:"
   read bytes
   [ -z "${bytes}" ] && bytes=3000
   local size=$((bytes/15)) # aproximation
   export ST_REQUEST_BODY="{"$(k=0 ; while [ $k -lt $size ]; do k=$((k+1)); echo -n "\"id${RANDOM}\":${RANDOM}"; [ ${k} -lt $size ] && echo -n "," ; done)"}"
   echo "Random request created has $(echo ${ST_REQUEST_BODY} | wc -c) bytes (~ ${bytes})"
   echo "If you need as file: echo \${ST_REQUEST_BODY} > request-${bytes}b.json"
}

# Invoke the function:
random_request


Input Request url
 (or set 'ST_REQUEST_URL' to be non-interactive) [/app/v1/load-test/v1/id-21]:

Server configuration:
{"preReserveRequestBody":true,"receiveRequestBody":true}
Server data configuration:
{"purgeExecution":false,"storeEvents":false,"storeEventsKeyHistory":false}

Removing current server data information ... done !

Input Launcher type (h2load|hermes|h2client)
 (or set 'ST_LAUNCHER' to be non-interactive) [h2load]: h2load

Input Number of h2load iterations
 (or set 'H2LOAD__ITERATIONS' to be non-interactive) [100000]: 100000

Input Number of h2load clients
 (or set 'H2LOAD__CLIENTS' to be non-interactive) [1]: 1

Input Number of h2load threads
 (or set 'H2LOAD__THREADS' to be non-interactive) [1]: 1

Input Number of h2load concurrent streams
 (or set 'H2LOAD__CONCURRENT_STREAMS' to be non-interactive) [100]: 100


+ h2load -t1 -n100000 -c1 -m100 http://0.0.0.0:8000/load-test/v1/id-21 -d /tmp/tmp.6ad32NuVqJ/request.json
+ tee -a ./report_delay0_iters100000_c1_t1_m100.txt
starting benchmark...
spawning thread #0: 1 total client(s). 100000 total requests
Application protocol: h2c
progress: 10% done
progress: 20% done
progress: 30% done
progress: 40% done
progress: 50% done
progress: 60% done
progress: 70% done
progress: 80% done
progress: 90% done
progress: 100% done

finished in 784.31ms, 127501.09 req/s, 133.03MB/s
requests: 100000 total, 100000 started, 100000 done, 100000 succeeded, 0 failed, 0 errored, 0 timeout
status codes: 100000 2xx, 0 3xx, 0 4xx, 0 5xx
traffic: 104.34MB (109407063) total, 293.03KB (300058) headers (space savings 95.77%), 102.33MB (107300000) data
                     min         max         mean         sd        +/- sd
time for request:      230us     11.70ms       757us       215us    91.98%
time for connect:      136us       136us       136us         0us   100.00%
time to 1st byte:     1.02ms      1.02ms      1.02ms         0us   100.00%
req/s           :  127529.30   127529.30   127529.30        0.00   100.00%

real    0m0,790s
user    0m0,217s
sys     0m0,073s
+ set +x

Created test report:
  last -> ./report_delay0_iters100000_c1_t1_m100.txt
```

## Execution of main agent

### Command line

You may take a look to `h2agent` command line by just typing the build path, for example for `Release` target using native executable:

<details>
<summary>h2agent --help</summary>

```bash
$ build/Release/bin/h2agent --help
h2agent - HTTP/2 Agent service

Usage: h2agent [options]

Options:

[--name <name>]
  Application/process name. Used in prometheus metrics 'source' label. Defaults to 'h2agent'.

[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]
  Set the logging level; defaults to warning.

[-v|--verbose]
  Output log traces on console.

[--ipv6]
  IP stack configured for IPv6. Defaults to IPv4.

[-b|--bind-address <address>]
  Servers local bind <address> (admin/traffic/prometheus); defaults to '0.0.0.0' (ipv4) or '::' (ipv6).

[-a|--admin-port <port>]
  Admin local <port>; defaults to 8074.

[-p|--traffic-server-port <port>]
  Traffic server local <port>; defaults to 8000. Set '0' (or negative) to
  disable (mock server service is enabled by default).

[-m|--traffic-server-api-name <name>]
  Traffic server API name; defaults to empty.

[-n|--traffic-server-api-version <version>]
  Traffic server API version; defaults to empty.

[-w|--traffic-server-worker-threads <threads>]
  Number of traffic server worker threads; defaults to 1 (inline processing,
  no queue dispatcher). When set to 1, requests are processed directly within
  the nghttp2 I/O thread, which is optimal for fast provisions (e.g. regex
  matching with static responses). When set above 1, a queue dispatcher model
  is activated: I/O threads enqueue requests and worker threads process them
  asynchronously. This helps when provision logic is slow (response delays,
  file I/O, etc.) as it keeps I/O threads responsive. For trivial logic, the
  dispatch overhead may negate any benefit. Admin server hardcodes 1 worker
  thread(s).

[--traffic-server-max-worker-threads <threads>]
  Maximum number of worker threads; defaults to '--traffic-server-worker-threads'.
  When set higher, additional threads are created on demand to handle traffic
  spikes. Only effective when queue dispatcher is active (workers > 1).

[-t|--traffic-server-io-threads <threads>]
  Number of nghttp2 traffic server I/O threads; defaults to 1.
  Connections are assigned to threads round-robin, so multiple
  threads only help when multiple client connections are used.
  Admin server hardcodes 1 nghttp2 thread(s).

[--traffic-server-queue-dispatcher-max-size <size>]
  The queue dispatcher model (which is activated for more than 1 server worker)
  schedules a initial number of threads which could grow up to a maximum value
  (given by '--traffic-server-max-worker-threads').
  Optionally, a basic congestion control algorithm can be enabled by mean providing
  a non-negative value to this parameter. When the queue size grows due to lack of
  consumption capacity, a service unavailable error (503) will be answered skipping
  context processing when the queue size reaches the value provided; defaults to -1,
  which means that congestion control is disabled.

[-k|--traffic-server-key <path file>]
  Path file for traffic server key to enable SSL/TLS; insecured by default.

[-d|--traffic-server-key-password <password>]
  When using SSL/TLS this may provided to avoid 'PEM pass phrase' prompt at process
  start.

[-c|--traffic-server-crt <path file>]
  Path file for traffic server crt to enable SSL/TLS; insecured by default.

[-s|--secure-admin]
  When key (-k|--traffic-server-key) and crt (-c|--traffic-server-crt) are provided,
  only traffic interface is secured by default. This option secures admin interface
  reusing traffic configuration (key/crt/password).

[--schema <path file>]
  Path file for optional startup schema configuration.

[--global-variable <path file>]
  Path file for optional startup global variable(s) configuration.

[--traffic-server-matching <path file>]
  Path file for optional startup traffic server matching configuration.

[--traffic-server-provision <path file>]
  Path file for optional startup traffic server provision configuration.

[--traffic-client-endpoint <path file>]
  Path file for optional startup traffic client endpoint configuration.

[--traffic-client-provision <path file>]
  Path file for optional startup traffic client provision configuration.

[--traffic-server-ignore-request-body]
  Ignores traffic server request body reception processing as optimization in
  case that its content is not required by planned provisions (enabled by default).

[--traffic-server-dynamic-request-body-allocation]
  When data chunks are received, the server appends them into the final request body.
  In order to minimize reallocations over internal container, a pre reserve could be
  executed (by design, the maximum received request body size is allocated).
  Depending on your traffic profile this could be counterproductive, so this option
  disables the default behavior to do a dynamic reservation of the memory.

[--discard-data]
  Disables data storage for events processed (enabled by default).
  This invalidates some features like FSM related ones (in-state, out-state)
  or event-source transformations.
  This affects to both mock server-data and client-data storages,
  but normally both containers will not be used together in the same process instance.

[--discard-data-key-history]
  Disables data key history storage (enabled by default).
  Only latest event (for each key '[client endpoint/]method/uri')
  will be stored and will be accessible for further analysis.
  This limits some features like FSM related ones (in-state, out-state)
  or event-source transformations or client triggers.
  Implicitly disabled by option '--discard-data'.
  Ignored for server-unprovisioned events (for troubleshooting purposes).
  This affects to both mock server-data and client-data storages,
  but normally both containers will not be used together in the same process instance.

[--disable-purge]
  Skips events post-removal when a provision on 'purge' state is reached (enabled by default).

  This affects to both mock server-data and client-data purge procedures,
  but normally both flows will not be used together in the same process instance.

  In server mode, purge clears events for the current data key (method + URI).
  In client mode, purge clears all events accumulated during the entire state chain,
  where each step typically targets a different method+URI (see docs/api/README.md).

[--prometheus-port <port>]
  Prometheus local <port>; defaults to 8080.

[--prometheus-response-delay-seconds-histogram-boundaries <comma-separated list of doubles>]
  Bucket boundaries for response delay seconds histogram; no boundaries are defined by default.
  Scientific notation is allowed, i.e.: "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3".
  This affects to both mock server and client processing time values, but normally both flows
  will not be used together in the same process instance. On the server, it's primarily aimed
  at controlling local bottlenecks, so it makes more sense to use it on the client endpoint.

[--prometheus-message-size-bytes-histogram-boundaries <comma-separated list of doubles>]
  Bucket boundaries for Rx/Tx message size bytes histogram; no boundaries are defined by default.
  This affects to both mock 'server internal/client external' message size values,
  but normally both flows will not be used together in the same process instance.

[--disable-metrics]
  Disables prometheus scrape port (enabled by default).

[--long-term-files-close-delay-usecs <microseconds>]
  Close delay after write operation for those target files with constant paths provided.
  Normally used for logging files: we should have few of them. By default, 1000000
  usecs are configured. Delay is useful to avoid I/O overhead under normal conditions.
  Zero value means that close operation is done just after writting the file.

[--short-term-files-close-delay-usecs <microseconds>]
  Close delay after write operation for those target files with variable paths provided.
  Normally used for provision debugging: we could have multiple of them. Traffic rate
  could constraint the final delay configured to avoid reach the maximum opened files
  limit allowed. By default, it is configured to 0 usecs.
  Zero value means that close operation is done just after writting the file.

[--remote-servers-lazy-connection]
  By default connections are performed when adding client endpoints.
  This option configures remote addresses to be connected on demand.

[--traffic-client-worker-threads <threads>]
  Number of traffic client worker threads per endpoint; defaults to 1.
  Each worker creates its own HTTP/2 connection to the endpoint.
  Requests are dispatched round-robin (sequence % threads) across
  workers, parallelizing response processing (transforms, JsonConstraint,
  etc.) while a single timer drives the configured RPS rate.

[-V|--version]
  Program version.

[-h|--help]
  This help.

Typical use cases:

  Mock server (fast static responses):
    h2agent [--traffic-server-io-threads <N>]
    Default settings are optimal. Use '-t <N>' with N matching the number
    of client connections to distribute I/O load across threads.

  Mock server (simulated latency or heavy transforms):
    h2agent -w <N> [--traffic-server-max-worker-threads <M>]
    Workers handle slow provisions without blocking I/O threads.
    Add '--traffic-server-queue-dispatcher-max-size <S>' to enable
    congestion control (503 responses when queue exceeds S).

  Traffic client (load generator):
    h2agent --traffic-server-port 0 --traffic-client-worker-threads <N>
    Disable server with port 0. Each worker opens its own connection,
    multiplying effective throughput.

  Benchmark:
    h2agent --verbose --prometheus-response-delay-seconds-histogram-boundaries
      "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3"
    Enables detailed latency histograms for performance analysis.
```

</details>

## Execution of matching helper utility

This utility could be useful to test regular expressions before putting them at provision objects (`requestUri` or transformation filters which use regular expressions).

### Command line

You may take a look to `matching-helper` command line by just typing the build path, for example for `Release` target using native executable:

<details>
<summary>matching-helper --help</summary>

```bash
$ build/Release/bin/matching-helper --help
Usage: matching-helper [options]

Options:

-r|--regex <value>
  Regex pattern value to match against.

-t|--test <value>
  Test string value to be matched.

[-f|--fmt <value>]
  Optional regex-replace output format.

[-h|--help]
  This help.

Examples:
   matching-helper --regex "https://(\w+).(com|es)/(\w+)/(\w+)" \
                   --test "https://github.com/testillano/h2agent" --fmt 'User: $3; Project: $4'
   matching-helper --regex "(a\|b\|)([0-9]{10})" --test "a|b|0123456789" --fmt '$2'
   matching-helper --regex "1|3|5|9" --test 2
```

</details>

Execution example:

```bash
$ build/Release/bin/matching-helper --regex "(a\|b\|)([0-9]{10})" --test "a|b|0123456789" --fmt '$2'

Regex: (a\|b\|)([0-9]{10})
Test:  a|b|0123456789
Fmt:   $2

Match result: true
Fmt result  : 0123456789
```

## Execution of Arash Partow's helper utility

This utility could be useful to test [Arash Partow's](https://github.com/ArashPartow/exprtk) mathematical expressions before putting them at provision objects (`math.*` source).

### Command line

You may take a look to `arashpartow-helper` command line by just typing the build path, for example for `Release` target using native executable:

<details>
<summary>arashpartow-helper --help</summary>

```bash
$ build/Release/bin/arashpartow-helper --help
Usage: arashpartow-helper [options]

Options:

-e|--expression <value>
  Expression to be calculated.

[-h|--help]
  This help.

Examples:
   arashpartow-helper --expression "(1+sqrt(5))/2"
   arashpartow-helper --expression "404 == 404"
   arashpartow-helper --expression "cos(3.141592)"

Arash Partow help: https://raw.githubusercontent.com/ArashPartow/exprtk/master/readme.txt
```

</details>

Execution example:

```bash
$ build/Release/bin/arashpartow-helper --expression "404 == 404"

Expression: 404 == 404

Result: 1
```

## Execution of h2client utility

This utility could be useful to test simple HTTP/2 requests.

### Command line

You may take a look to `h2client` command line by just typing the build path, for example for `Release` target using native executable:

<details>
<summary>h2client --help</summary>

```bash
$ build/Release/bin/h2client --help
Usage: h2client [options]

Options:

-u|--uri <value>
 URI to access.

[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]
  Set the logging level; defaults to warning.

[-v|--verbose]
  Output log traces on console.

[-t|--timeout-milliseconds <value>]
  Time in milliseconds to wait for requests response. Defaults to 5000.

[-m|--method <POST|GET|PUT|DELETE|HEAD>]
  Request method. Defaults to 'GET'.

[--header <value>]
  Header in the form 'name:value'. This parameter can occur multiple times.

[-b|--body <value>]
  Plain text for request body content.

[--secure]
 Use secure connection.

[--rc-probe]
  Forwards HTTP status code into equivalent program return code.
  So, any code greater than or equal to 200 and less than 400
  indicates success and will return 0 (1 in other case).
  This allows to use the client as HTTP/2 command probe in
  kubernetes where native probe is only supported for HTTP/1.

[-h|--help]
  This help.

Examples:
   h2client --timeout 1 --uri http://localhost:8000/book/8472098362
   h2client --method POST --header "content-type:application/json" --body '{"foo":"bar"}' --uri http://localhost:8000/data
```

</details>

Execution example:

```bash
$ build/Release/bin/h2client --timeout 1 --uri http://localhost:8000/book/8472098362

Client endpoint:
   Secure connection: false
   Host:   localhost
   Port:   8000
   Method: GET
   Uri: http://localhost:8000/book/8472098362
   Path:   book/8472098362
   Timeout for responses (ms): 5000


 Response status code: 200
 Response body: {"author":"Ludwig von Mises"}
 Response headers: [date: Sun, 27 Nov 2022 18:58:32 GMT]
```

## UDP utilities

> **Note:** Since `h2agent` now supports native HTTP/2 client capabilities and the `clientProvision` target (which triggers outgoing HTTP/2 flows directly from server transformations), most functional testing scenarios that previously required the UDP channel can be solved entirely within `h2agent`. The UDP tools below are primarily useful for **benchmarking** (controlled-rate load generation via `udp-client` + `udp-server-h2client`) and for **integration with external non-HTTP systems** that need to react to `h2agent` events through the `udpSocket.*` target. Among them, `udp-server-h2client` remains the most versatile, as it bridges UDP events to HTTP/2 requests towards isolated services.

## Execution of udp-server utility

This utility could be useful to test UDP messages sent by `h2agent` (`udpSocket.*` target).
You can also use netcat in bash, to generate messages easily:

```bash
echo -n "<message here>" | nc -u -q0 -w1 -U /tmp/udp.sock
```

### Command line

You may take a look to `udp-server` command line by just typing the build path, for example for `Release` target using native executable:

<details>
<summary>udp-server --help</summary>

```bash
$ build/Release/bin/udp-server --help
Usage: udp-server [options]

Options:

-k|--udp-socket-path <value>
  UDP unix socket path.

[-e|--print-each <value>]
  Print messages each specific amount (must be positive). Defaults to 1.
  Setting datagrams estimated rate should take 1 second/printout and output
  frequency gives an idea about UDP receptions rhythm.

[-h|--help]
  This help.

Examples:
   udp-server --udp-socket-path /tmp/udp.sock

To stop the process you can send UDP message 'EOF':
   echo -n EOF | nc -u -q0 -w1 -U /tmp/udp.sock
```

</details>

Execution example:

```bash
$ build/Release/bin/udp-server --udp-socket-path /tmp/udp.sock

Path: /tmp/udp.sock
Print each: 1 message(s)

Remember:
 To stop process: echo -n EOF | nc -u -q0 -w1 -U /tmp/udp.sock


Waiting for UDP messages...

<timestamp>                         <sequence>      <udp datagram>
___________________________________ _______________ _______________________________
2023-08-02 19:16:36.340339 GMT      0               555000000
2023-08-02 19:16:37.340441 GMT      1               555000001
2023-08-02 19:16:38.340656 GMT      2               555000002

Exiting (EOF received) !
```

## Execution of udp-server-h2client utility

This utility could be useful to test UDP messages sent by `h2agent` (`udpSocket.*` target).
You can also use netcat in bash, to generate messages easily:

```bash
echo -n "<message here>" | nc -u -q0 -w1 -U /tmp/udp.sock
```

The difference with previous `udp-server` utility, is that this can trigger actively HTTP/2 requests for every UDP reception.
This makes possible coordinate actions between `h2agent` acting as a server, to create outgoing requests linked to its receptions through the UDP channel served in this external tool.
Powerful parsing capabilities allow to create any kind of request dynamically using patterns `@{udp[.n]}` for uri, headers and body configured.
Prometheus metrics are also available to measure the HTTP/2 performance towards the remote server (check it by mean, for example: `curl http://0.0.0.0:8081/metrics`).

### Command line

You may take a look to `udp-server-h2client` command line by just typing the build path, for example for `Release` target using native executable:

<details>
<summary>udp-server-h2client --help</summary>

```bash
$ build/Release/bin/udp-server-h2client --help
Usage: udp-server-h2client [options]

Options:

UDP server will trigger one HTTP/2 request for every reception, replacing optionally
certain patterns on method, uri, headers and/or body provided. Implemented patterns:
following:

   @{udp}:      replaced by the whole UDP datagram received.
   @{udp8}:     selects the 8 least significant digits in the UDP datagram, and may
                be used to build valid IPv4 addresses for a given sequence.
   @{udp.<n>}:  UDP datagram received may contain a pipe-separated list of tokens
                and this pattern will be replaced by the nth one.
   @{udp8.<n>}: selects the 8 least significant digits in each part if exists.

To stop the process you can send UDP message 'EOF'.
To print accumulated statistics you can send UDP message 'STATS' or stop/interrupt the process.

[--name <name>]
  Application/process name. Used in prometheus metrics 'source' label. Defaults to 'udp-server-h2client'.

-k|--udp-socket-path <value>
  UDP unix socket path.

[-o|--udp-output-socket-path <value>]
  UDP unix output socket path. Written for every response received. This socket must be previously created by UDP server (bind()).
  Try this bash recipe to create an UDP server socket (or use another udp-server-h2client instance for that):
     $ path="/tmp/udp2.sock"
     $ rm -f ${path}
     $ socat -lm -ly UNIX-RECV:"${path}" STDOUT

[--udp-output-value <value>]
  UDP datagram to be written on output socket, for every response received. By default,
  original received datagram is used (@{udp}). Same patterns described above are valid for this parameter.

[-w|--workers <value>]
  Number of worker threads to post outgoing requests and manage asynchronous timers (timeout, pre-delay).
  Defaults to system hardware concurrency (8), however 2 could be enough.

[-e|--print-each <value>]
  Print UDP receptions each specific amount (must be positive). Defaults to 1.
  Setting datagrams estimated rate should take 1 second/printout and output
  frequency gives an idea about UDP receptions rhythm.

[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]
  Set the logging level; defaults to warning.

[-v|--verbose]
  Output log traces on console.

[-t|--timeout-milliseconds <value>]
  Time in milliseconds to wait for requests response. Defaults to 5000.

[-d|--send-delay-milliseconds <value>]
  Time in seconds to delay before sending the request. Defaults to 0.
  It also supports negative values which turns into random number in
  the range [0,abs(value)].

[-m|--method <value>]
  Request method. Defaults to 'GET'. After optional parsing, should be one of:
  POST|GET|PUT|DELETE|HEAD.

-u|--uri <value>
 URI to access.

[--header <value>]
  Header in the form 'name:value'. This parameter can occur multiple times.

[-b|--body <value>]
  Plain text for request body content.

[--secure]
 Use secure connection.

[--prometheus-bind-address <address>]
  Prometheus local bind <address>; defaults to 0.0.0.0.

[--prometheus-port <port>]
  Prometheus local <port>; defaults to 8081. Value of -1 disables metrics.

[--prometheus-response-delay-seconds-histogram-boundaries <comma-separated list of doubles>]
  Bucket boundaries for response delay seconds histogram; no boundaries are defined by default.
  Scientific notation is allowed, i.e.: "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3".

[--prometheus-message-size-bytes-histogram-boundaries <comma-separated list of doubles>]
  Bucket boundaries for Tx/Rx message size bytes histogram; no boundaries are defined by default.

[-h|--help]
  This help.

Examples:
   udp-server-h2client --udp-socket-path /tmp/udp.sock --print-each 1000 --timeout-milliseconds 1000 --uri http://0.0.0.0:8000/book/@{udp} --body "ipv4 is @{udp8}"
   udp-server-h2client --udp-socket-path /tmp/udp.sock --print-each 1000 --method POST --uri http://0.0.0.0:8000/data --header "content-type:application/json" --body '{"book":"@{udp}"}'

   To provide body from file, use this trick: --body "$(jq -c '.' long-body.json)"
```

</details>

Execution example:

```bash
$ build/Release/bin/udp-server-h2client -k /tmp/udp.sock -t 3000 -d -300 -u http://0.0.0.0:8000/data --header "content-type:application/json" -b '{"foo":"@{udp}"}'

Application/process name: udp-server-h2client
UDP socket path: /tmp/udp.sock
Workers: 80
Print each: 1 message(s)
Log level: Warning
Verbose (stdout): false
Workers: 10
Maximum workers: 40
Congestion control is disabled
Prometheus local bind address: 0.0.0.0
Prometheus local port: 8081
Client endpoint:
   Secure connection: false
   Host:   0.0.0.0
   Port:   8000
   Method: GET
   Uri: http://0.0.0.0:8000/data
   Path:   data
   Headers: [content-type: application/json]
   Body: {"foo":"@{udp}"}
   Timeout for responses (ms): 3000
   Send delay for requests (ms): random in [0,300]
   Builtin patterns used: @{udp}

Remember:
 To get prometheus metrics:       curl http://localhost:8081/metrics
 To send ad-hoc UDP message:      echo -n <data> | nc -u -q0 -w1 -U /tmp/udp.sock
 To print accumulated statistics: echo -n STATS  | nc -u -q0 -w1 -U /tmp/udp.sock
 To stop process:                 echo -n EOF    | nc -u -q0 -w1 -U /tmp/udp.sock


Waiting for UDP messages...

<timestamp>                         <sequence>      <udp datagram>                  <accumulated status codes>
___________________________________ _______________ _______________________________ ___________________________________________________________
2023-08-02 19:16:36.340339 GMT      0               555000000                       0 2xx, 0 3xx, 0 4xx, 0 5xx, 0 timeouts, 0 connection errors
2023-08-02 19:16:37.340441 GMT      1               555000001                       1 2xx, 0 3xx, 0 4xx, 0 5xx, 0 timeouts, 0 connection errors
2023-08-02 19:16:38.340656 GMT      2               555000002                       2 2xx, 0 3xx, 0 4xx, 0 5xx, 0 timeouts, 0 connection errors

Exiting (EOF received) !

status codes: 3 2xx, 0 3xx, 0 4xx, 0 5xx, 0 timeouts, 0 connection errors
```

## Execution of udp-client utility

This utility could be useful to test `udp-server`, and specially, `udp-server-h2client` tool.
You can also use netcat in bash, to generate messages easily, but this tool provide high load. This tool manages a monotonically increasing sequence within a given range, and allow to parse it over a pattern to build the datagram generated. Even, we could provide a list of patterns which will be randomized.
Although we could launch multiple UDP clients towards the UDP server (such server must be unique due to non-oriented connection nature of UDP protocol), it is probably unnecessary: this client is fast enough to generate the required load.

### Command line

You may take a look to `udp-client` command line by just typing the build path, for example for `Release` target using native executable:

<details>
<summary>udp-client --help</summary>

```bash
$ build/Release/bin/udp-client --help
Usage: udp-client [options]

Options:

-k|--udp-socket-path <value>
  UDP unix socket path.

[--eps <value>]
  Events per second. Floats are allowed (0.016667 would mean 1 tick per minute),
  negative number means unlimited (depends on your hardware) and 0 is prohibited.
  Defaults to 1.

[-r|--rampup-seconds <value>]
  Rampup seconds to reach 'eps' linearly. Defaults to 0.
  Only available for speeds over 1 event per second.

[-i|--initial <value>]
  Initial value for datagram. Defaults to 0.

[-f|--final <value>]
  Final value for datagram. Defaults to unlimited.

[--template <value>]
  Template to build UDP datagram (patterns '@{seq}' and '@{seq[<+|-><integer>]}'
  will be replaced by sequence number and shifted sequences respectively).
  Defaults to '@{seq}'.
  This parameter can occur multiple times to create a random set. For example,
  passing '--template foo --template foo --template bar', there is a probability
  of 2/3 to select 'foo' and 1/3 to select 'bar'.

[-e|--print-each <value>]
  Print messages each specific amount (must be positive). Defaults to 1.

[-h|--help]
  This help.

Examples:
   udp-client --udp-socket-path /tmp/udp.sock --eps 3500 --initial 555000000 --final 555999999 --template "foo/bar/@{seq}"
   udp-client --udp-socket-path /tmp/udp.sock --eps 3500 --initial 555000000 --final 555999999 --template "@{seq}|@{seq-8000}"
   udp-client --udp-socket-path /tmp/udp.sock --final 0 --template STATS # sends 1 single datagram 'STATS' to the server

To stop the process, just interrupt it.
```

</details>

Execution example:

```bash
$ build/Release/bin/udp-client --udp-socket-path /tmp/udp.sock --eps 1000 --initial 555000000 --print-each 1000

Path: /tmp/udp.sock
Print each: 1 message(s)
Range: [0, 18446744073709551615]
Pattern: @{seq}
Events per second: 1000
Rampup (s): 0


Generating UDP messages...

<timestamp>                         <time(s)> <sequence>      <udp datagram>
___________________________________ _________ _______________ _______________________________
2023-08-02 19:16:36.340339 GMT      0         0               555000000
2023-08-02 19:16:37.340441 GMT      1         1000            555000999
2023-08-02 19:16:38.340656 GMT      2         2000            555001999
...

```

## Working with unix sockets and docker containers

In former sections we described the UDP utilities available at `h2agent`project. But we run them natively. As they are packaged into `h2agent` docker image, they can also be launched as docker containers selecting the appropriate entry point. The only thing to take into account is that the unix socket between UDP server (`udp-server` or `udp-server-h2client`) and client (`udp-client`) must be shared. This can be done through two alternatives:

* Executing client and server within the same container.
* Executing them in separate containers (recommended as docker best practice "one container - one process").

Taking `udp-server` and `udp-client` as example:

In the **first case**, we will launch the second one (client) in foreground using `docker exec`:

```bash
$ docker run -d --rm -it --name udp --entrypoint /opt/udp-server ghcr.io/testillano/h2agent:latest -k /tmp/udp.sock
$ docker exec -it udp /opt/udp-client -k /tmp/udp.sock # in foreground will throw client output
```

If the client is launched in background (-d) you won't be able to follow process output (`docker logs -f udp` shows server output because it was launched in first place).

In the **second case**, which is the recommended, we need to create an external volume:

```bash
$ docker volume create --name=socketVolume
```

And then, we can run the containers in separated shells (or both in background with '-d' because know they have independent docker logs):

```bash
$ docker run --rm -it -v socketVolume:/tmp --entrypoint /opt/udp-server ghcr.io/testillano/h2agent:latest -k /tmp/udp.sock
```

```bash
$ docker run --rm -it -v socketVolume:/tmp --entrypoint /opt/udp-client ghcr.io/testillano/h2agent:latest -k /tmp/udp.sock
```

This can also be done with `docker-compose`:

```yaml
version: '3.3'

volumes:
  socketVolume:
    external: true

services:
  udpServer:
    image: ghcr.io/testillano/h2agent:latest
    volumes:
      - socketVolume:/tmp
    entrypoint: ["/opt/udp-server"]
    command: ["-k", "/tmp/udp.sock"]

  udpClient:
    image: ghcr.io/testillano/h2agent:latest
    depends_on:
      - udpServer
    volumes:
      - socketVolume:/tmp
    entrypoint: ["/bin/bash", "-c"] # we can also use bash entrypoint to ease command:
    command: >
      "/opt/udp-client -k /tmp/udp.sock"
```



## Execution with TLS support

`H2agent` server mock supports `SSL/TLS`. You may use helpers located under `tools/ssl` to create key and certificate files for client and server:

```bash
$ ls tools/ssl/
create_ca-signed-certificates.sh  create_self-signed_certificates.sh
```

Once executed, a hint will show how to proceed, mainly adding these parameters to the `h2agent`:

```bash
--traffic-server-key <server key file> --traffic-server-crt <server certificate file> --traffic-server-key-password <key password to avoid PEM Phrase prompt on startup>
```

As well as some `curl` hints (secure and insecure examples).

## Metrics

Based in [prometheus data model](https://prometheus.io/docs/concepts/data_model/) and implemented with [prometheus-cpp library](https://github.com/jupp0r/prometheus-cpp), those metrics are collected and exposed through the server scraping port (`8080` by default, but configurable at [command line](#Command-line) by mean `--prometheus-port` option) and could be retrieved using Prometheus or compatible visualization software like [Grafana](https://prometheus.io/docs/visualization/grafana/) or just browsing `http://localhost:8080/metrics`.

More information about implemented metrics [here](#OAM).
To play with grafana automation in `h2agent` project, go to `./tools/grafana` directory and check its [PLAY_GRAFANA.md](./tools/grafana/PLAY_GRAFANA.md) file to learn more about.

## Traces and printouts

Traces are managed by `syslog` by default, but could be shown verbosely at standard output (`--verbose`) depending on the traces design level and the current level assigned. For example:

```bash
$ ./h2agent --verbose &
[1] 27407


88            ad888888b,
88           d8"     "88                                                     ,d
88                   a8P                                                     88
88,dPPYba,        ,d8P"   ,adPPYYba,   ,adPPYb,d8   ,adPPYba,  8b,dPPYba,  MM88MMM
88P'    "8a     a8P"      ""     `Y8  a8"    `Y88  a8P_____88  88P'   `"8a   88
88       88   a8P'        ,adPPPPP88  8b       88  8PP"""""""  88       88   88
88       88  d8"          88,    ,88  "8a,   ,d88  "8b,   ,aa  88       88   88,
88       88  88888888888  `"8bbdP"Y8   `"YbbdP"Y8   `"Ybbd8"'  88       88   "Y888
                                       aa,    ,88
                                        "Y8bbdP"

https://github.com/testillano/h2agent

Quick Start:    https://github.com/testillano/h2agent#quick-start
Prezi overview: https://prezi.com/view/RFaiKzv6K6GGoFq3tpui/
ChatGPT:        https://github.com/testillano/h2agent/blob/master/README.md#questions-and-answers


20/11/22 20:53:33 CET: Starting h2agent
Log level: Warning
Verbose (stdout): true
IP stack: IPv4
Admin local port: 8074
Traffic server (mock server service): enabled
Traffic server local bind address: 0.0.0.0
Traffic server local port: 8000
Traffic server api name: <none>
Traffic server api version: <none>
Traffic server worker threads: 1
Traffic server key password: <not provided>
Traffic server key file: <not provided>
Traffic server crt file: <not provided>
SSL/TLS disabled: both key & certificate must be provided
Traffic secured: no
Admin secured: no
Schema configuration file: <not provided>
Global variables configuration file: <not provided>
Traffic server process request body: true
Traffic server pre reserve request body: true
Data storage: enabled
Data key history storage: enabled
Purge execution: enabled
Traffic server matching configuration file: <not provided>
Traffic server provision configuration file: <not provided>
Prometheus local bind address: 0.0.0.0
Prometheus local port: 8080
Long-term files close delay (usecs): 1000000
Short-term files close delay (usecs): 0
Remote servers lazy connection: false

$ kill $!
20/11/22 20:53:37 CET: [Warning]|/code/src/main.cpp:207(sighndl)|Signal received: 15
20/11/22 20:53:37 CET: [Warning]|/code/src/main.cpp:194(myExit)|Terminating with exit code 1
20/11/22 20:53:37 CET: [Warning]|/code/src/main.cpp:148(stopAgent)|Stopping h2agent timers service at 20/11/22 20:53:37 CET
20/11/22 20:53:37 CET: [Warning]|/code/src/main.cpp:154(stopAgent)|Stopping h2agent admin service at 20/11/22 20:53:37 CET
20/11/22 20:53:37 CET: [Warning]|/code/src/main.cpp:161(stopAgent)|Stopping h2agent traffic service at 20/11/22 20:53:37 CET
20/11/22 20:53:37 CET: [Warning]|/code/src/main.cpp:198(myExit)|Stopping logger

[1]+  Exit 1                  h2agent --verbose
```

## Training

### Prepare the environment

#### Working in project checkout

##### Requirements

Some utilities may be required, so please try to install them on your system. For example:

```bash
$ sudo apt-get install netcat
$ sudo apt-get install curl
$ sudo apt-get install jq
$ sudo apt-get install dos2unix
```

##### Starting agent

Then you may build project images and start the `h2agent` with its docker image:

```bash
$ ./build.sh --auto # builds project images
$ ./run.sh --verbose # starts agent with docker by mean helper script
```

Or build native executable and run it from shell:

```bash
$ ./build-native.sh # builds executable
$ build/Release/bin/h2agent --verbose # starts executable
```

#### Working in training container

The training image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull ghcr.io/testillano/h2agent_training:<tag>
```

Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.

You may also find useful run the training image by mean the helper script `./tools/training.sh`. This script builds and runs an image based in `./Dockerfile.training` which adds the needed resources to run training resources. The image working directory is `/home/h2agent` making the experience like working natively over the git checkout and providing by mean symbolic links, main project executables.

If your are working in the training container, there is no need to build the project neither install requirements commented in previous section, just execute the process in background:

```bash
bash-5.1# ls -lrt
total 12
drwxr-xr-x    5 root     root          4096 Dec 16 20:29 tools
drwxr-xr-x   12 root     root          4096 Dec 16 20:29 kata
drwxr-xr-x    2 root     root          4096 Dec 16 20:29 demo
lrwxrwxrwx    1 root     root            12 Dec 16 20:29 h2agent -> /opt/h2agent
bash-5.1# ./h2agent --verbose &
```

### Training resources

#### Questions and answers

A conversational bot is available in `./tools/questions-and-answers` directory. It is implemented in python using *langchain* and *OpenAI* (ChatGPT) technology. Also *Groq* model can be used if the proper key is detected. Check its [README.md](./tools/questions-and-answers/README.md) file to learn more about.

#### Play

A playground is available at `./tools/play-h2agent` directory. It is designed to guide through a set of easy examples. Check its [README.md](./tools/play-h2agent/README.md) file to learn more about.

#### Test

A GUI tester implemented in python is available at `./tools/test-h2agent` directory. It is designed to make quick interactions through traffic and administrative interfaces. Check its [README.md](./tools/test-h2agent/README.md) file to learn more about.

#### Demo

A demo is available at `./demo` directory. It is designed to introduce the `h2agent` in a funny way with an easy use case. Open its [README.md](./demo/README.md) file to learn more about.

#### Kata

A kata is available at `./kata` directory. It is designed to guide through a set of exercises with increasing complexity. Check its [README.md](./kata/README.md) file to learn more about.

## Management interface

`h2agent` listens on a specific management port (*8074* by default) for incoming requests, implementing a *REST API* to manage the process operation. Through the *API* we could program the agent behavior over *URI* path `/admin/v1/`.

The full API reference is documented using the [OpenAPI 3.1 specification](./docs/api/openapi.yaml) and rendered interactively at:

**[https://testillano.github.io/h2agent/api/](https://testillano.github.io/h2agent/api/)**

For detailed conceptual documentation (matching algorithms, state machines, transformation pipeline with sources/targets/filters, triggering, data querying), see the [API User Guide](./docs/api/README.md).

The API is organized in the following groups:

| Group | Endpoints | Description |
|-------|-----------|-------------|
| **schema** | `POST` `GET` `DELETE` `/admin/v1/schema` | Validation schemas for traffic checking |
| **global-variable** | `POST` `GET` `DELETE` `/admin/v1/global-variable` | Shared variables between provisions |
| **files** | `GET` `/admin/v1/files` | Processed files status |
| **logging** | `GET` `PUT` `/admin/v1/logging` | Dynamic log level configuration |
| **configuration** | `GET` `/admin/v1/configuration` | General process configuration |
| **server/configuration** | `GET` `PUT` `/admin/v1/server/configuration` | Server request body reception settings |
| **server-matching** | `POST` `GET` `/admin/v1/server-matching` | Traffic classification algorithm (FullMatching, FullMatchingRegexReplace, RegexMatching) |
| **server-provision** | `POST` `GET` `DELETE` `/admin/v1/server-provision` | Server mock response behavior and transformation pipeline |
| **server-data** | `GET` `PUT` `DELETE` `/admin/v1/server-data` | Server events storage and inspection |
| **client-endpoint** | `POST` `GET` `DELETE` `/admin/v1/client-endpoint` | Remote server connection definitions |
| **client-provision** | `POST` `GET` `DELETE` `/admin/v1/client-provision` | Client mock request behavior, triggering and transformation pipeline |
| **client-data** | `GET` `PUT` `DELETE` `/admin/v1/client-data` | Client events storage and inspection |

## Server-triggered client flows with serverEvent source

When a server provision triggers a client provision (via `clientProvision.<id>` target), the client provision can read data directly from the originating server event using the `serverEvent` source. This avoids copying fields to intermediate `globalVar` variables and is the recommended approach when the client request must be built from server request data.

**Source syntax:** `serverEvent.<method>.<uri>.<event-number>.<json-path>`

**Example:** A webhook receiver that forwards the notification body to another endpoint:

```json
// server-provision.json
{
  "requestMethod": "POST",
  "requestUri": "/api/v1/webhook/notify",
  "responseCode": 200,
  "responseBody": {"status": "received"},
  "transform": [
    { "source": "value.1", "target": "clientProvision.forwardNotification.initial" }
  ]
}
```

```json
// client-provision.json
{
  "id": "forwardNotification",
  "endpoint": "myServer",
  "requestMethod": "POST",
  "requestUri": "/api/v1/forward",
  "requestHeaders": {"content-type": "application/json"},
  "transform": [
    {
      "source": "serverEvent.POST./api/v1/webhook/notify.0.body",
      "target": "request.body.json.object"
    }
  ]
}
```

The client provision reads the last received body at `POST /api/v1/webhook/notify` and uses it as the outgoing request body — no `globalVar` needed. A full runnable example is available at `tools/play-h2agent/examples/ServerTriggersClientViaServerEvent`.

> **Note:** `serverEvent` requires server-data storage to be enabled (default). If `--discard-data` is set, the source will fail and the transformation is skipped.

Similarly, server provisions can access client event history using the `clientEvent` source. This is useful when the server acts as an intermediary: it triggers a client flow, and then uses the client response data to build its own response. The `clientEvent` source uses query-parameter addressing with `clientEndpointId`, `requestMethod`, `requestUri`, `eventNumber` and `eventPath` fields (see the [transformation pipeline](docs/api/README.md#transformation-pipeline) section for details).

## Dynamic response delays

The provisioning model allows configuration of the response delay, in milliseconds, for a received request. This delay may be a fixed or a random value, but is always a single, static delay overall. However, an additional mechanism -the dynamic delay- can be employed as a pseudo-notification procedure to suppress (or block) answers under specific conditions. This feature is activated via a global variable with the naming format: `__core.response-delay-ms.<recvseq>`.

This variable holds a millisecond value that postpones the answer for a specific reception identifier. The dynamic delay is ignored (and the answer is immediate) if the variable does not exist, holds an invalid (non-numeric) value, or a zeroed value. This procedure operates independently of the provisioned response delay, which is executed first (if configured).

The simplest way to use this feature is to configure the server provisioning to create this variable using the received server sequence as the unique reception identifier:

```json
{
  "requestMethod": "GET",
  "requestUri": "/foo/bar",
  "responseCode": 200,
  "responseDelayMs": 20,
  "responseHeaders": {
    "content-type": "text/html"
  },
  "responseBody": "done!",
  "transform": [
    {
      "source": "recvseq",
      "target": "var.recvseq"
    },
    {
      "source": "value.1",
      "target": "globalVar.__core.response-delay-ms.@{recvseq}"
    }
  ]
}
```

In that use case, when a GET request is received by the server, its own dynamic response delay is created with a value of 1 millisecond. You could confirm that just checking global variables hold by the h2agent process:

```bash
$ curl -s --http2-prior-knowledge http://localhost:8074/admin/v1/global-variable | jq '.'
{
  "__core.response-delay-ms.1": "1"
}
```

The procedure is as follows: the answer is first delayed by 20 ms (as configured in the provisioning model). Subsequently, the dynamic mechanism begins: the server checks the variable's value, which is currently 1 ms, thereby updating the timer expiration repeatedly until the variable is updated to zero milliseconds (an invalid value will also release the wait loop), or until the variable is removed. Caution should be exercised with small delay values, as they could provoke a burst of timer events in the server when checking the answer condition, especially if that condition takes too long to resolve (and the client-side request timeout is also large).

The server provisioning model can modify the variable upon subsequent receptions (it may need to correlate information from those requests with an auxiliary storage where the original server sequence is kept). However, these updates are typically managed externally through the administrative interface, for example:

```bash
$ curl --http2-prior-knowledge -XPOST http://localhost:8074/admin/v1/global-variable -H'content-type:application/json' -d'{"__core.response-delay-ms.1":"0"}'
- or better: -
$ curl --http2-prior-knowledge -XDELETE "http://localhost:8074/admin/v1/global-variable?name=__core.response-delay-ms.1"
```

How the server sequence is determined is a separate issue. For example, the provisioning configuration could write UDP datagrams (containing the server sequence), which might trigger other external operations that ultimately lead to those administrative operations.

## Reserved Global Variables

The following variables are used internally by the server engine (`__core`) to manage critical functions such as dynamic response latency and stream error handling. These variables **must not be manipulated or overwritten** by user configurations for different purposes as expected, to avoid interference.

---

### 1. Dynamic Response Delay

| Variable | Description |
| :--- | :--- |
| `__core.response-delay-ms.<recvseq>` | Stores a dynamic delay value (in milliseconds) that **postpones the response** to a request. It functions as a pseudo-notification mechanism. If the value is zero, non-numeric, or the variable is removed, the dynamic delay is ignored. <u>This is an "input" variable</u> as it is used to feed a procedure. |

#### Components

| Component | Example Value | Description |
| :--- | :--- | :--- |
| `__core` | N/A | **Reserved Prefix:** Indicates the variable belongs to the core system and is reserved. |
| `response-delay-ms` | N/A | **Reserved Function:** Identifies the variable as the dynamic response delay (in milliseconds). |
| `<recvseq>` | `12345` | **Sequence Identifier Value:** The unique reception sequence value (`server sequence`) for the specific request. |

---

### 2. Stream Error Indicator

| Variable | Description |
| :--- | :--- |
| `__core.stream-error-traffic-server.<recvseq>.<method>.<uri>` | Stores an [error code](https://datatracker.ietf.org/doc/html/rfc7540#section-7) indicating a stream or connection error detected from traffic server. Used to notify external failure conditions that must be reflected in the response. <u>This is an "output" variable</u> as it is dumped automatically on errors. |

#### Components

| Component | Example Value | Description |
| :--- | :--- | :--- |
| `__core` | N/A | **Reserved Prefix:** Indicates the variable belongs to the core system and is reserved. |
| `stream-error-traffic-server` | N/A | **Reserved Function:** Identifies the variable as a stream or connection error indicator. |
| `<recvseq>` | `12345` | **Sequence Identifier Value:** The unique reception sequence value (`server sequence`) for the request. |
| `<method>` | `GET` | **Request Method Value:** The method used in the HTTP request (e.g., GET, POST, PUT, etc.). |
| `<uri>` | `/api/users` | **Request Uri Value:** The Uniform Resource Identifier (URI) or path of the HTTP request. |

## How it is delivered

`h2agent` is delivered in a `helm` chart called `h2agent` (`./helm/h2agent`) so you may integrate it in your regular `helm` chart deployments by just adding a few artifacts.
This chart deploys the `h2agent` pod based on the docker image with the executable under `./opt` together with some helper functions to be sourced on docker shell: `/opt/utils/helpers.bash` (default directory path can be modified through `utilsMountPath` helm chart value).
Take as example the component test chart `ct-h2agent` (`./helm/ct-h2agent`), where the main chart is added as a file requirement but could also be added from helm repository:

## How it integrates in a service

1. Add the project's [helm repository](https://testillano.github.io/helm/) with alias `erthelm`:

   ```bash
    helm repo add erthelm https://testillano.github.io/helm
   ```

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

     - name: h2agent
       version: 1.0.0
       repository: alias:erthelm
       alias: h2client
   ```

3. Refer to `h2agent` values through the corresponding dependency alias, for example `.Values.h2server.image` to access process repository and tag.

### Agent configuration files

Some [command line](#Command-line) arguments used by the `h2agent` process are files, so they could be added by mean a `config map` (key & certificate for secured connections and matching/provision configuration files).

## Troubleshooting

### Helper functions

As we commented [above](#How-it-is-delivered), the `h2agent` helm chart packages a helper functions script which is very useful for troubleshooting. This script is also available for native usage (`./tools/helpers.bash`):

```bash
$ source ./tools/helpers.bash

===== h2agent operation helpers =====
Shortcut helpers (sourced variables and functions)
to ease agent operation over management interface:
   https://github.com/testillano/h2agent#management-interface

=== Common variables & functions ===
SERVER_ADDR=localhost
TRAFFIC_PORT=8000
ADMIN_PORT=8074
METRICS_PORT=8080
CURL="curl -i --http2-prior-knowledge"
traffic_url(): http://localhost:8000
admin_url():   http://localhost:8074/admin/v1
metrics_url(): http://localhost:8080/metrics

=== General ===
Usage: schema [-h|--help] [--clean] [file]; Cleans/gets/updates current schema configuration
                                            (http://localhost:8074/admin/v1/schema).
Usage: global_variable [-h|--help] [--clean] [name|file]; Cleans/gets/updates current agent global variable configuration
                                                          (http://localhost:8074/admin/v1/global-variable).
Usage: files [-h|--help]; Gets the files processed.
Usage: files_configuration [-h|--help]; Manages files configuration (gets current status by default).
                            [--enable-read-cache]  ; Enables cache for read operations.
                            [--disable-read-cache] ; Disables cache for read operations.
Usage: udp_sockets [-h|--help]; Gets the sockets for UDP processed.
Usage: configuration [-h|--help]; Gets agent general static configuration.

=== Traffic server ===
Usage: server_configuration [-h|--help]; Manages agent server configuration (gets current status by default).
       [--traffic-server-ignore-request-body]  ; Ignores request body on server receptions.
       [--traffic-server-receive-request-body] ; Processes request body on server receptions.
       [--traffic-server-dynamic-request-body-allocation] ; Does dynamic request body memory allocation on server receptions.
       [--traffic-server-initial-request-body-allocation] ; Pre reserves request body memory on server receptions.
Usage: server_data_configuration [-h|--help]; Manages agent server data configuration (gets current status by default).
                                 [--discard-all]     ; Discards all the events processed.
                                 [--discard-history] ; Keeps only the last event processed for a key.
                                 [--keep-all]        ; Keeps all the events processed.
                                 [--disable-purge]   ; Skips events post-removal when a provision on 'purge' state is reached.
                                 [--enable-purge]    ; Processes events post-removal when a provision on 'purge' state is reached.

Usage: server_matching [-h|--help] [file]; Gets/updates current server matching configuration
                                           (http://localhost:8074/admin/v1/server-matching).
Usage: server_provision [-h|--help] [--clean] [file]; Cleans/gets/updates current server provision configuration
                                                      (http://localhost:8074/admin/v1/server-provision).
Usage: server_provision_unused [-h|--help]; Get current server provision configuration still not used
                                                      (http://localhost:8074/admin/v1/server-provision/unused).
Usage: server_data [-h|--help]; Inspects server data events (http://localhost:8074/admin/v1/server-data).
                   [method] [uri] [[-]event number] [event path] ; Restricts shown data with given positional filters.
                                                                   Event number may be negative to access by reverse
                                                                   chronological order.
                   [--summary] [max keys]          ; Gets current server data summary to guide further queries.
                                                     Displayed keys (method/uri) could be limited (10 by default, -1: no limit).
                   [--clean] [query filters]       ; Removes server data events. Admits additional query filters to narrow the
                                                     selection.
                   [--surf]                        ; Interactive sorted (regardless method/uri) server data navigation.
                   [--dump]                        ; Dumps all sequences detected for server data under 'server-data-sequences'
                                                     directory.
=== Traffic client ===
Usage: client_endpoint [-h|--help] [--clean] [file]; Cleans/gets/updates current client endpoint configuration
                                                     (http://localhost:8074/admin/v1/client-endpoint).
Usage: client_provision [-h|--help] [--clean]; Cleans/gets/updates/triggers current client provision configuration
                                               (http://localhost:8074/admin/v1/client-provision).
                                       [file]; Configure client provision by mean json specification.
                        [id] [id query param]; Triggers client provision identifier and optionally provide dynamics
                                               configuration (omit with empty value):
                                               [inState, sequence (sync), sequenceBegin, sequenceEnd, rps, repeat (true|false)]
Usage: client_provision_unused [-h|--help]; Get current client provision configuration still not used
                                            (http://localhost:8074/admin/v1/client-provision/unused).
Usage: client_data [-h|--help]; Inspects client data events (http://localhost:8074/admin/v1/client-data).
                   [client endpoint id] [method] [uri] [[-]event number] [event path] ; Restricts shown data with given
                                                                                        positional filters.
                                                                                        Event number may be negative to
                                                                                        access by reverse chronological order.
                   [--summary] [max keys]          ; Gets current client data summary to guide further queries.
                                                     Displayed keys (client endpoint id/method/uri) could be limited
                                                     (10 by default, -1: no limit).
                   [--clean] [query filters]       ; Removes client data events. Admits additional query filters to narrow
                                                     the selection.
                   [--surf]                        ; Interactive sorted (regardless endpoint/method/uri) client data navigation.
                   [--dump]                        ; Dumps all sequences detected for client data under 'client-data-sequences'
                                                     directory.
=== Schemas ===
Usage: schema_schema [-h|--help]; Gets the schema configuration schema
                                  (http://localhost:8074/admin/v1/schema/schema).
Usage: global_variable_schema [-h|--help]; Gets the agent global variable configuration schema
                                           (http://localhost:8074/admin/v1/global-variable/schema).
Usage: server_matching_schema [-h|--help]; Gets the server matching configuration schema
                                           (http://localhost:8074/admin/v1/server-matching/schema).
Usage: server_provision_schema [-h|--help]; Gets the server provision configuration schema
                                            (http://localhost:8074/admin/v1/server-provision/schema).
Usage: client_endpoint_schema [-h|--help]; Gets the client endpoint configuration schema
                                           (http://localhost:8074/admin/v1/client-endpoint/schema).
Usage: client_provision_schema [-h|--help]; Gets the client provision configuration schema
                                            (http://localhost:8074/admin/v1/client-provision/schema).
=== Auxiliary ===
Usage: pretty [-h|--help]; Beautifies json content for last operation response.
              [jq expression, '.' by default]; jq filter over previous content.
              Example filter: schema && pretty '.[] | select(.id=="myRequestsSchema")'
Usage: raw [-h|--help]; Outputs raw json content for last operation response.
           [jq expression, '.' by default]; jq filter over previous content.
           Example filter: schema && raw '.[] | select(.id=="myRequestsSchema")'
Usage: trace [-h|--help] [level: Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency]; Gets/sets h2agent
                                                                                                     tracing level.
Usage: metrics [-h|--help]; Prometheus metrics.
Usage: snapshot [-h|--help]; Creates a snapshot directory with process data & configuration.
Usage: server_example [-h|--help]; Basic server configuration examples. Try: source <(server_example)
Usage: client_example [-h|--help]; Basic client configuration examples. Try: source <(client_example)
Usage: help; This help. Overview: help | grep ^Usage
```

### OAM

You could use any visualization framework to analyze metrics information from `h2agent` but perhaps the simplest way to do it is using the `metrics` function  (just a direct `curl` command to the scrape port) from [function helpers](#Helper-functions): `metrics`.

So, a direct scrape (for example towards the agent after its *component test*) would be something like this:

```bash
$ kubectl exec -it -n ns-ct-h2agent h2agent-55b9bd8d4d-2hj9z -- sh -c "curl http://localhost:8080/metrics"
```

On native execution, it is just a simple `curl` native request:

```bash
$ curl http://localhost:8080/metrics
```

Metrics implemented could be divided **counters**, **gauges** or **histograms**:

- **Counters**:
  - Processed requests (successful/errored(service unavailable, method not allowed, method not implemented, wrong api name or version, unsupported media type)/unsent(conection error))
  - Processed responses (successful/timed-out)
  - Non provisioned requests
  - Purged contexts (successful/failed)
  - File system and Unix sockets operations (successful/failed)


- **Gauges and histograms**:

  - Response delay seconds
  - Message size bytes for receptions
  - Message size bytes for transmissions



The metrics naming in this project, includes a family prefix which is the project applications name (`h2agent` or `udp_server_h2client`) and the endpoint category (`traffic_server`, `admin_server`, `traffic_client` for `h2agent`, and empty (as implicit), for `udp-server-h2client`). This convention and the labels provided (`[label] `: source, method, status_code, rst_stream_goaway_error_code, operation, result), are designed to ease metrics identification when using monitoring systems like [grafana](https://www.grafana.com).

The label ''**source**'': one of these labels is the source of information, which could be optionally dynamic (if `--name` parameter is provided to the applications, so we could have `h2agent` by default, or `h2agentB` to be more specific, although grafana provides the `instance` label anyway), or dynamic anyway for the case of client endpoints, which provisioned names are also part of source label.

In general: `source value = <process name>[_<endpoint identifier>]`, where the endpoint identifier has sense for `h2agent` clients as multiple client endpoints could be provisioned. For example:

* No process name provided:

  * h2agent (traffic_server/admin_server/file_manager/socket_manager, are part of the family name).
  * h2agent_myClient (traffic_client is part of family name)
  * udp-server-h2client (we omit endpoint identifier, as unique and implicit in default process name)
* Process name provided (`--name h2agentB` or `--name udp-server-h2clientB`):

  * h2agentB (traffic_server/admin_server/file_manager/socket_manager, are part of the family name).
  * h2agentB_myClient (traffic_client is part of family name)
  * udp-server-h2clientB (we omit endpoint identifier, as unique and <u>should be implicit</u> in process name)



These are the groups of metrics implemented in the project:



#### HTTP/2 clients

```
Counters provided by http2comm library and h2agent itself(*):

   h2agent_traffic_client_observed_requests_sents_counter [source] [method]
   h2agent_traffic_client_observed_requests_unsent_counter [source] [method]
   h2agent_traffic_client_observed_responses_received_counter [source] [method] [status_code] [rst_stream_goaway_error_code]
   h2agent_traffic_client_observed_responses_timedout_counter [source] [method]
   h2agent_traffic_client_provisioned_requests_counter (*) [source] [result: successful/failed]
   h2agent_traffic_client_purged_contexts_counter (*) [source] [result: successful/failed]
   h2agent_traffic_client_unexpected_response_status_code_counter (*) [source]

Gauges provided by http2comm library:

   h2agent_traffic_client_responses_delay_seconds_gauge [source] [method] [status_code] [rst_stream_goaway_error_code]
   h2agent_traffic_client_sent_messages_size_bytes_gauge [source] [method]
   h2agent_traffic_client_received_messages_size_bytes_gauge [source] [method] [status_code] [rst_stream_goaway_error_code]

Histograms provided by http2comm library:

   h2agent_traffic_client_responses_delay_seconds [source] [method] [status_code] [rst_stream_goaway_error_code]
   h2agent_traffic_client_sent_messages_size_bytes [source] [method]
   h2agent_traffic_client_received_messages_size_bytes [source] [method] [status_code] [rst_stream_goaway_error_code]
```



As commented, same metrics described above, are also generated for the other application 'udp-server-h2client':



```
Counters provided by http2comm library:

   udp_server_h2client_observed_requests_sents_counter [source] [method]
   udp_server_h2client_observed_requests_unsent_counter [source] [method]
   udp_server_h2client_observed_responses_received_counter [source] [method] [status_code] [rst_stream_goaway_error_code]
   udp_server_h2client_observed_responses_timedout_counter [source] [method]

Gauges provided by http2comm library:

   udp_server_h2client_responses_delay_seconds_gauge [source] [method] [status_code] [rst_stream_goaway_error_code]
   udp_server_h2client_sent_messages_size_bytes_gauge [source] [method]
   udp_server_h2client_received_messages_size_bytes_gauge [source] [method] [status_code] [rst_stream_goaway_error_code]

Histograms provided by http2comm library:

   udp_server_h2client_responses_delay_seconds [source] [method] [status_code] [rst_stream_goaway_error_code]
   udp_server_h2client_sent_messages_size_bytes [source] [method]
   udp_server_h2client_received_messages_size_bytes [source] [method] [status_code] [rst_stream_goaway_error_code]
```



Examples:

```bash
udp_server_h2client_responses_delay_seconds_bucket{source="customer",method="POST",status_code="201",le="0.005"} 52
h2agent_traffic_client_observed_responses_timedout_counter{source="http2proxy_myClient",method="POST"} 1
h2agent_traffic_client_observed_responses_received_counter{source="h2agent_myClient",method="POST",status_code="201"} 9776
```

Note that 'histogram' is not part of histograms' category metrics name suffix (as counters and gauges do). The reason is to avoid confusion because metrics created are not actually histogram containers (except bucket). So, 'sum' and 'count' can be used to represent latencies, but not directly as histograms but doing some intermediate calculations:

```bash
rate(h2agent_traffic_client_responses_delay_seconds_sum[2m])/rate(h2agent_traffic_client_responses_delay_seconds_count[2m])
```

So, previous expression (rate is the mean variation in given time interval) is better without 'histogram' in the names, and helps to represent the latency updated in real time (every 2 minutes in the example).

#### HTTP/2 servers

We have two groups of server metrics. One for administrative operations (1 administrative server interface) and one for traffic events (1 traffic server interface):

```
Counters provided by http2comm library and h2agent itself(*):

   h2agent_[traffic|admin]_server_observed_requests_accepted_counter [source] [method]
   h2agent_[traffic|admin]_server_observed_requests_errored_counter [source] [method]
   h2agent_[traffic|admin]_server_observed_responses_counter [source] [method] [status_code] [rst_stream_goaway_error_code]
   h2agent_traffic_server_provisioned_requests_counter (*) [source] [result: successful/failed]
   h2agent_traffic_server_purged_contexts_counter (*) [source] [result: successful/failed]

Gauges provided by http2comm library:

   h2agent_[traffic|admin]_server_responses_delay_seconds_gauge [source] [method] [status_code] [rst_stream_goaway_error_code]
   h2agent_[traffic|admin]_server_received_messages_size_bytes_gauge [source] [method]
   h2agent_[traffic|admin]_server_sent_messages_size_bytes_gauge [source] [method] [status_code] [rst_stream_goaway_error_code]

Histograms provided by http2comm library:

   h2agent_[traffic|admin]_server_responses_delay_seconds [source] [method] [status_code] [rst_stream_goaway_error_code]
   h2agent_[traffic|admin]_server_received_messages_size_bytes [source] [method]
   h2agent_[traffic|admin]_server_sent_messages_size_bytes [source] [method] [status_code] [rst_stream_goaway_error_code]
```

For example:

```bash
h2agent_traffic_server_received_messages_size_bytes_bucket{source="myServer",method="POST",status_code="201",le="322"} 38
h2agent_traffic_server_provisioned_requests_counter{source="h2agent",result="failed"} 234
h2agent_traffic_server_purged_contexts_counter{source="h2agent",result="successful"} 2361
```

#### File system

```
Counters provided by h2agent:

   h2agent_file_manager_operations_counter [source] [operation: open/close/write/empty/delayedClose/instantClose] [result: successful/failed]
```

For example:

```bash
h2agent_file_manager_operations_counter{source="h2agent",operation="open",result="failed"} 0
```

#### UDP via sockets

```
Counters provided by h2agent:

   h2agent_socket_manager_operations_counter [source] [operation: open/write/delayedWrite/instantWrite] [result: successful/failed]
```

For example:

```bash
h2agent_socket_manager_operations_counter{source="myServer",operation="write",result="successful"} 25533
```



## Contributing

Check the project [contributing guidelines](./CONTRIBUTING.md).
