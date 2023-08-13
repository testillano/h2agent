# C++ HTTP/2 Mock Service

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://codedocs.xyz/testillano/h2agent.svg)](https://codedocs.xyz/testillano/h2agent/index.html)
[![Coverage Status](https://coveralls.io/repos/github/testillano/h2agent/badge.svg?branch=master&kill_cache=1)](https://coveralls.io/github/testillano/h2agent?branch=master)
[![Ask Me Anything !](https://img.shields.io/badge/Ask%20me-anything-1abc9c.svg)](https://github.com/testillano)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/testillano/h2agent/graphs/commit-activity)
[![CI](https://github.com/testillano/h2agent/actions/workflows/ci.yml/badge.svg)](https://github.com/testillano/h2agent/actions/workflows/ci.yml)

`H2agent` is a network service agent that enables **mocking other network services using HTTP/2 protocol**.
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

  * HTTP/2.
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
    * Filters: regular expression captures and regex/replace, append, prepend, basic arithmetics (sum, multiply), equality, condition variables, differences, json constraints.
    * Targets: dynamic variables, global variables, files (write), response body (as string, integer, unsigned, float, boolean, object and object from json string), unix socket UDP (write), response body path (as string, integer, unsigned, float, boolean, object and object from json string), headers, status code, response delay, output state, events, break conditions.
  * Multipart support.

* Training:

  * Questions and answers for project documentation using **openai** (ChatGPT-based).
  * Playground.
  * Demo.
  * Kata exercises.

* Tools programs:

  * Matching helper.
  * Arash Partow helper (math expressions).
  * HTTP/2 client.
  * UDP server.
  * UDP server to trigger active HTTP/2 client requests.
  * UDP client.



## Quick start

**Theory**

* A ***[prezi](https://prezi.com/view/RFaiKzv6K6GGoFq3tpui/)*** presentation to show a complete and useful overview of the `h2agent` component architecture.
* A conversational bot for [***questions & answers***](./README.md#questions-and-answers) based in *Open AI*.

**Practice**

* Brief exercises to ***[play](./README.md#Play)*** with, showing basic configuration "games" to have a quick overview of project possibilities.
* A ***[demo](./README.md#Demo)*** exercise which presents a basic use case to better understand the project essentials.
* And finally, a ***[kata](./README.md#Kata)*** training to acquire better knowledge of project capabilities.

Bullet list of exercises above, have a growing demand in terms of attention and dedicated time. For that reason, they are presented in the indicated order, facilitating and prioritizing simplicity for the user in the training process.

## Scope

When developing a network service, one often needs to integrate it with other services. However, integrating full-blown versions of such services in a development setup is not always suitable, for instance when they are either heavyweight or not fully developed.

`H2agent` can be used to replace one (or many) of those, which allows development to progress and testing to be conducted in isolation against such a service.

`H2agent` supports HTTP2 as a network protocol and JSON as a data interchange language.

So, `h2agent` could be used as:

* **Server** mock: fully implemented.
* **Client** mock: partially implemented (new features ongoing).

Also, `h2agent` can be configured through **command-line** but also dynamically through an **administrative HTTP/2 interface** (`REST API`). This last feature makes the process a key element within an ecosystem of remotely controlled agents, enabling a reliable and powerful orchestration system to develop all kinds of functional, load and integration tests. So, in summary `h2agent` offers two execution planes:

* **Traffic plane**: application flows.
* **Control plane**: traffic flow orchestration, mocks behavior control and SUT surroundings monitoring and inspection.

Check the [releases](https://github.com/testillano/h2agent/releases) to get latest packages, or read the following sections to build all the artifacts needed to start playing:

## How can you use it ?

`H2agent` process (as well as other project binaries) may be used natively, as a `docker` container, or as part of `kubernetes` deployment.

The easiest way to build the project is using [containers](https://en.wikipedia.org/wiki/LXC) technology (this project uses `docker`): **to generate all the artifacts**, just type the following:

```bash
$> ./build.sh --auto
```

The option `--auto` builds the <u>builder image</u> (`--builder-image`) , then the <u>project image</u> (`--project-image`) and finally <u>project executables</u> (`--project`). Then you will have everything available to run binaries with different modes:

* Run <u>project image</u> with docker (`./h2a.sh` script at root directory can also be used):

  ```bash
  $> docker run --rm -it -p 8000:8000 -p 8074:8074 -p 8080:8080 ghcr.io/testillano/h2agent:latest # default entrypoint is h2agent process
  ```

  Exported ports correspond to server defaults: traffic(8000), administrative(8074) and metrics(8080).
  You may override default entrypoint (`/opt/h2agent`) to run another binary packaged (check project `Dockerfile`), for example the simple client utility:

  ```bash
  $> docker run --rm -it --network=host --entrypoint "/opt/h2client" ghcr.io/testillano/h2agent:latest --uri http://localhost:8000/unprovisioned # run in another shell to get response from h2agent server launched above
  ```

  Or any other packaged utility (if you want to lighten the image size, write your own Dockerfile and get what you need):

  ```bash
  $> docker run --rm -it --network=host --entrypoint "/opt/matching-helper" ghcr.io/testillano/h2agent:latest --help
  -or-
  $> docker run --rm -it --network=host --entrypoint "/opt/arashpartow-helper" ghcr.io/testillano/h2agent:latest --help
  -or-
  $> docker run --rm -it --network=host --entrypoint "/opt/h2client" ghcr.io/testillano/h2agent:latest --help
  -or-
  $> docker run --rm -it --network=host --entrypoint "/opt/udp-server" ghcr.io/testillano/h2agent:latest --help
  -or-
  $> docker run --rm -it --network=host --entrypoint "/opt/udp-server-h2client" ghcr.io/testillano/h2agent:latest --help
  -or-
  $> docker run --rm -it --network=host --entrypoint "/opt/udp-client" ghcr.io/testillano/h2agent:latest --help
  ```

* Run within `kubernetes` deployment: corresponding `helm charts` are normally packaged into releases. This is described in ["how it is delivered"](#How-it-is-delivered) section, but in summary, you could do the following:

  ```bash
  $> # helm dependency update helm/h2agent # no dependencies at the moment
  $> helm install h2agent-example helm/h2agent --wait
  $> pod=$(kubectl get pod -l app.kubernetes.io/name=h2agent --no-headers -o name)
  $> kubectl exec ${pod} -c h2agent -- /opt/h2agent --help # run, for example, h2agent help
  ```

  You may enter the pod and play with helpers functions and examples (deployed with the chart under `/opt/utils`) which are anyway, automatically sourced on `bash` shell:

  ```bash
  $> kubectl exec -it ${pod} -- bash
  ```

It is also possible to build the project natively (not using containers) installing all the dependencies on the local host:

```bash
$> ./build-native.sh # you may prepend non-empty DEBUG variable value in order to troubleshoot build procedure
```

So, you could run `h2agent` (or any other binary available under `build/<build type>/bin`) directly:


* Run <u>project executable</u> natively (standalone):

  ```bash
  $> build/Release/bin/h2agent & # default server at 0.0.0.0 with traffic/admin/prometheus ports: 8000/8074/8080
  ```

  Provide `-h` or `--help` to get **process help** (more information [here](#Execution-of-main-agent)) or execute any other project executable.

  You may also play with project helpers functions and examples:

  ```bash
  $> source tools/helpers.src # type help in any moment after sourcing
  $> server_example # follow instructions or just source it: source <(server_example)
  $> client_example # follow instructions or just source it: source <(client_example)
  ```


## Static linking

Both build helpers (`build.sh` and `build-native.sh` scripts) allow to force project static link, although this is [not recommended](https://stackoverflow.com/questions/57476533/why-is-statically-linking-glibc-discouraged):

```bash
$> STATIC_LINKING=TRUE ./build.sh --auto
- or -
$> STATIC_LINKING=TRUE ./build-native.sh
```

So, you could run binaries regardless if needed libraries are available or not (including `glibc` with all its drawbacks).




Next sections will describe in detail, how to build [project image](#Project-image) and project executables ([using docker](#Build-project-with-docker) or [natively](#Build-project-natively)).

## Project image

This image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$> docker pull ghcr.io/testillano/h2agent:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$> ./build.sh --project-image
```

This image is built with `./Dockerfile`.
Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.
If you want to work with alpine-based images, you may build everything from scratch, including all docker base images which are project dependencies.

## Build project with docker

### Builder image

This image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$> docker pull ghcr.io/testillano/h2agent_builder:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$> ./build.sh --builder-image
```

This image is built with `./Dockerfile.build`.
Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.
If you want to work with alpine-based images, you may build everything from scratch, including all docker base images which are project dependencies.

### Usage

Builder image is used to build the project. To run compilation over this image, again, just run with `docker`:

```bash
$> envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$> docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
          ghcr.io/testillano/h2agent_builder:<tag>
```

You could generate documentation passing extra arguments to the [entry point](https://github.com/testillano/nghttp2/blob/master/deps/build.sh) behind:

```bash
$> docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
          ghcr.io/testillano/h2agent_builder::<tag> "" doc
```

You could also build the library using the script `./build.sh` located at project root:


```bash
$> ./build.sh --project
```

## Build project natively

It may be hard to collect every dependency, so there is a native build **automation script**:

```bash
$> ./build-native.sh
```

Note 1: this script is tested on `ubuntu bionic`, then some requirements could be not fulfilled in other distributions.

Note 2: once dependencies have been installed, you may just type `cmake . && make` to have incremental native builds.

Note 3: if not stated otherwise, this document assumes that binaries (used on examples) are natively built.



Anyway, we will describe the common steps for a `cmake-based` building project like this. Firstly you may install `cmake`:

```bash
$> sudo apt-get install cmake
```

And then generate the makefiles from project root directory:

```bash
$> cmake .
```

You could specify type of build, 'Debug' or 'Release', for example:

```bash
$> cmake -DCMAKE_BUILD_TYPE=Debug .
$> cmake -DCMAKE_BUILD_TYPE=Release .
```

You could also change the compilers used:

```bash
$> cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++     -DCMAKE_C_COMPILER=/usr/bin/gcc
```

or

```bash
$> cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang
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
$> make
```

### Clean

```bash
$> make clean
```

### Documentation

```bash
$> make doc
```

```bash
$> cd docs/doxygen
$> tree -L 1
     .
     ├── Doxyfile
     ├── html
     ├── latex
     └── man
```

### Install

```bash
$> sudo make install
```

Optionally you could specify another prefix for installation:

```bash
$> cmake -DMY_OWN_INSTALL_PREFIX=$HOME/applications/http2
$> make install
```

### Uninstall

```bash
$> cat install_manifest.txt | sudo xargs rm
```

## Testing

### Unit test

Check the badge above to know the current coverage level.
You can execute it after project building, for example for `Release` target:

```bash
$> build/Release/bin/unit-test # native executable
- or -
$> docker run -it --rm -v ${PWD}/build/Release/bin/unit-test:/ut --entrypoint "/ut" ghcr.io/testillano/h2agent:latest # docker
```

To shortcut docker run execution, `./ut.sh` script at root directory can also be used.
You may provide extra arguments to Google test executable, for example:

```bash
$> ./ut.sh --gtest_list_tests # to list the available tests
$> ./ut.sh --gtest_filter=Transform_test.ResponseBodyHexString # to filter and run 1 specific test
$> ./ut.sh --gtest_filter=Transform_test.* # to filter and run 1 specific suite
etc.
```

#### Coverage

Unit test coverage could be easily calculated executing the script `./tools/coverage.sh`. This script builds and runs an image based in `./Dockerfile.coverage` which uses the `lcov` utility behind. Finally, a `firefox` instance is launched showing the coverage report where you could navigate the source tree to check the current status of the project. This stage is also executed as part of `h2agent` continuous integration (`github workflow`).

Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.
If you want to work with alpine-based images, you may build everything from scratch, including all docker base images which are project dependencies.

### Component test

Component test is based in `pytest` framework. Just execute `ct/test.sh` to deploy the component test chart. Some cloud-native technologies are required: `docker`, `kubectl`, `minikube` and `helm`, for example:

```bash
$> docker version
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

$> kubectl version
Client Version: version.Info{Major:"1", Minor:"22", GitVersion:"v1.22.4", GitCommit:"b695d79d4f967c403a96986f1750a35eb75e75f1", GitTreeState:"clean", BuildDate:"2021-11-17T15:48:33Z", GoVersion:"go1.16.10", Compiler:"gc", Platform:"linux/amd64"}
Server Version: version.Info{Major:"1", Minor:"22", GitVersion:"v1.22.2", GitCommit:"8b5a19147530eaac9476b0ab82980b4088bbc1b2", GitTreeState:"clean", BuildDate:"2021-09-15T21:32:41Z", GoVersion:"go1.16.8", Compiler:"gc", Platform:"linux/amd64"}

$> minikube version
minikube version: v1.23.2
commit: 0a0ad764652082477c00d51d2475284b5d39ceed

$> helm version
version.BuildInfo{Version:"v3.7.1", GitCommit:"1d11fcb5d3f3bf00dbe6fe31b8412839a96b3dc4", GitTreeState:"clean", GoVersion:"go1.16.9"}
```

### Benchmarking test

This test is useful to identify possible memory leaks, process crashes or performance degradation introduced with new fixes or features.

Reference:

* VirtualBox VM with Linux Bionic (Ubuntu 18.04.3 LTS).

* Running on Intel(R) Core(TM) i7-8650U CPU @1.90GHz.

* Memory size: 15GiB.



Load testing is done with both [h2load](https://nghttp2.org/documentation/h2load-howto.html) and [hermes](https://github.com/jgomezselles/hermes) utilities using the helper script `st/start.sh` (check `-h|--help` for more information). Client capabilities benchmarking is done towards the `h2agent` itself, so we also could select `h2agent` with a simple client provision to work as the former utilities.

Also, `st/repeat.sh` script repeats a previous execution (last by default) in headless mode.

#### Considerations

* As schema validation is normally used only for function tests, it will be disabled here.
* `h2agent` could be for example started with 5 worker threads to discard application bottlenecks.
* Add histogram boundaries to better classify internal answer latencies for [metrics](#OAM).
* Data storage is disabled in the script by default to prevent memory from growing and improve server response times (remember that storage shall be kept when provisions require data persistence).
* In general, even with high traffic rates, you could get sneaky snapshots just enabling and then quickly disabling data storage, for example using [function helpers](#Helper-functions): `server_data_configuration --keep-all && server_data_configuration --discard-all`



So you may start the process, again, natively or using docker:

```bash
$> OPTS=(--verbose --traffic-server-worker-threads 5 --prometheus-response-delay-seconds-histogram-boundaries "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3")
$> build/Release/bin/h2agent "${OPTS[@]}" # native executable
- or -
$> docker run --rm -it --network=host -v $(pwd -P):$(pwd -P) ghcr.io/testillano/h2agent:latest "${OPTS[@]}" # docker
```

In other shell we launch the benchmark test:

```bash
$> st/start.sh -y


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

Input Launcher type (h2load|hermes)
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

```bash
$> build/Release/bin/h2agent --help
h2agent - HTTP/2 Agent service

Usage: h2agent [options]

Options:

[--name <name>]
  Application/process name. Used in prometheus metrics 'source' label. Defaults to 'h2agent'.

[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]
  Set the logging level; defaults to warning.

[--verbose]
  Output log traces on console.

[--ipv6]
  IP stack configured for IPv6. Defaults to IPv4.

[-b|--bind-address <address>]
  Servers local bind <address> (admin/traffic/prometheus); defaults to '0.0.0.0' (ipv4) or '::' (ipv6).

[-a|--admin-port <port>]
  Admin local <port>; defaults to 8074.

[-p|--traffic-server-port <port>]
  Traffic server local <port>; defaults to 8000. Set '-1' to disable
  (mock server service is enabled by default).

[-m|--traffic-server-api-name <name>]
  Traffic server API name; defaults to empty.

[-n|--traffic-server-api-version <version>]
  Traffic server API version; defaults to empty.

[-w|--traffic-server-worker-threads <threads>]
  Number of traffic server worker threads; defaults to 1, which should be enough
  even for complex logic provisioned (admin server hardcodes 1 worker thread(s)).
  It could be increased if hardware concurrency (8) permits a greater margin taking
  into account other process threads considered busy and I/O time spent by server
  threads. When more than 1 worker is configured, a queue dispatcher model starts
  to process the traffic, and also enables extra features like congestion control.

[--traffic-server-max-worker-threads <threads>]
  Number of traffic server maximum worker threads; defaults to the number of worker
  threads but could be a higher number so they will be created when needed to extend
  in real time, the queue dispatcher model capacity.

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
  Path file for traffic server key to enable SSL/TLS; unsecured by default.

[-d|--traffic-server-key-password <password>]
  When using SSL/TLS this may provided to avoid 'PEM pass phrase' prompt at process
  start.

[-c|--traffic-server-crt <path file>]
  Path file for traffic server crt to enable SSL/TLS; unsecured by default.

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

[--prometheus-port <port>]
  Prometheus local <port>; defaults to 8080.

[--prometheus-response-delay-seconds-histogram-boundaries <comma-separated list of doubles>]
  Bucket boundaries for response delay seconds histogram; no boundaries are defined by default.
  Scientific notation is allowed, i.e.: "100e-6,200e-6,300e-6,400e-6,1e-3,5e-3,10e-3,20e-3".
  This affects to both mock server-data and client-data processing time values,
  but normally both flows will not be used together in the same process instance.

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

[-v|--version]
  Program version.

[-h|--help]
  This help.
```

## Execution of matching helper utility

This utility could be useful to test regular expressions before putting them at provision objects (`requestUri` or transformation filters which use regular expressions).

### Command line

You may take a look to `matching-helper` command line by just typing the build path, for example for `Release` target using native executable:

```bash
$> build/Release/bin/matching-helper --help
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

Execution example:

```bash
$> build/Release/bin/matching-helper --regex "(a\|b\|)([0-9]{10})" --test "a|b|0123456789" --fmt '$2'

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

```bash
$> build/Release/bin/arashpartow-helper --help
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

Execution example:

```bash
$> build/Release/bin/arashpartow-helper --expression "404 == 404"

Expression: 404 == 404

Result: 1
```

## Execution of h2client utility

This utility could be useful to test simple HTTP/2 requests.

### Command line

You may take a look to `h2client` command line by just typing the build path, for example for `Release` target using native executable:

```bash
$> build/Release/bin/h2client --help
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

Execution example:

```bash
$> build/Release/bin/h2client --timeout 1 --uri http://localhost:8000/book/8472098362

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

## Execution of udp-server utility

This utility could be useful to test UDP messages sent by `h2agent` (`udpSocket.*` target).
You can also use netcat in bash, to generate messages easily:

```bash
echo -n "<message here>" | nc -u -q0 -w1 -U /tmp/udp.sock
```

### Command line

You may take a look to `udp-server` command line by just typing the build path, for example for `Release` target using native executable:

```bash
$> build/Release/bin/udp-server --help
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

Execution example:

```bash
$> build/Release/bin/udp-server --udp-socket-path /tmp/udp.sock

Path: /tmp/udp.sock
Print each: 1 message(s)

Remember:
 To stop process: echo -n EOF | nc -u -q0 -w1 -U /tmp/udp.sock


Waiting for UDP messages...

<timestamp>                         <sequence>      <udp datagram>
___________________________________ _______________ _______________________________
2023-08-02 19:16:36.340339 GMT      1               555000000
2023-08-02 19:16:37.340441 GMT      2               555000001
2023-08-02 19:16:38.340656 GMT      3               555000002

Existing (EOF received) !
```

## Execution of udp-server-h2client utility

This utility could be useful to test UDP messages sent by `h2agent` (`udpSocket.*` target).
You can also use netcat in bash, to generate messages easily:

```bash
echo -n "<message here>" | nc -u -q0 -w1 -U /tmp/udp.sock
```

The difference with previous `udp-server` utility, is that this can trigger actively HTTP/2 requests for ever UDP reception.
This makes possible coordinate actions between `h2agent` acting as a server, to create outgoing requests linked to its receptions through the UDP channel served in this external tool.
Powerful parsing capabilities allow to create any kind of request dynamically using patterns `@{udp[.n]}` for uri, headers and body configured.
Prometheus metrics are also available to measure the HTTP/2 performance towards the remote server (check it by mean, for example: `curl http://0.0.0.0:8081/metrics`).

### Command line

You may take a look to `udp-server-h2client` command line by just typing the build path, for example for `Release` target using native executable:

```bash
$> build/Release/bin/udp-server-h2client --help
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

To stop the process you can send UDP message 'EOF'.
To print accumulated statistics you can send UDP message 'STATS' or stop/interrupt the process.

[--name <name>]
  Application/process name. Used in prometheus metrics 'source' label. Defaults to 'udp-server-h2client'.

-k|--udp-socket-path <value>
  UDP unix socket path.

[-w|--workers <value>]
  Number of worker threads to post outgoing requests. By default, 10x times 'hardware
  concurrency' is configured (10*8 = 80), but you could consider increase even more
  if high I/O is expected (high response times raise busy threads, so context switching
  is not wasted as much as low latencies setups do). We should consider Amdahl law and
  other specific conditions to set the default value, but 10*CPUs is a good approach
  to start with. You may also consider using 'perf' tool to optimize your configuration.

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

Execution example:

```bash
$> build/Release/bin/udp-server-h2client -k /tmp/udp.sock -t 3000 -d -300 -u http://0.0.0.0:8000/data --header "content-type:application/json" -b '{"foo":"@{udp}"}'

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
 To print accumulated statistics: echo -n STATS | nc -u -q0 -w1 -U /tmp/udp.sock
 To stop process:                 echo -n EOF   | nc -u -q0 -w1 -U /tmp/udp.sock


Waiting for UDP messages...

<timestamp>                         <sequence>      <udp datagram>                  <accumulated status codes>
___________________________________ _______________ _______________________________ ___________________________________________________________
2023-08-02 19:16:36.340339 GMT      1               555000000                       0 2xx, 0 3xx, 0 4xx, 0 5xx, 0 timeouts, 0 connection errors
2023-08-02 19:16:37.340441 GMT      2               555000001                       1 2xx, 0 3xx, 0 4xx, 0 5xx, 0 timeouts, 0 connection errors
2023-08-02 19:16:38.340656 GMT      3               555000002                       2 2xx, 0 3xx, 0 4xx, 0 5xx, 0 timeouts, 0 connection errors

Existing (EOF received) !

status codes: 3 2xx, 0 3xx, 0 4xx, 0 5xx, 0 timeouts, 0 connection errors
```

## Execution of udp-client utility

This utility could be useful to test `udp-server`, and specially, `udp-server-h2client` tool.
You can also use netcat in bash, to generate messages easily, but this tool provide high load. This tool manages a monotonically increasing sequence within a given range, and allow to parse it over a pattern to build the datagram generated. Even, we could provide a list of patterns which will be randomized.
Although we could launch multiple UDP clients towards the UDP server (such server must be unique due to non-oriented connection nature of UDP protocol), it is probably unnecessary: this client is fast enough to generate the required load.

### Command line

You may take a look to `udp-client` command line by just typing the build path, for example for `Release` target using native executable:

```bash
$> build/Release/bin/udp-client --help
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

[--pattern <value>]
  Pattern to build UDP datagram (reserved @{seq} is replaced by sequence number).
  Defaults to '@{seq}'. This parameter can occur multiple times to create a random
  set. For example, passing '--pattern foo --pattern foo --pattern bar', there is a
  probability of 2/3 to select 'foo' and 1/3 to select 'bar'.

[-e|--print-each <value>]
  Print messages each specific amount (must be positive). Defaults to 1.

[-h|--help]
  This help.

Examples:
   udp-client --udp-socket-path /tmp/udp.sock --eps 3500 --initial 555000000 --final 555999999 --pattern "foo/bar/@{seq}"
   udp-client --udp-socket-path /tmp/udp.sock --final 0 --pattern STATS # sends 1 single datagram 'STATS' to the server

To stop the process, just interrupt it.
```

Execution example:

```bash
$> build/Release/bin/udp-client --udp-socket-path /tmp/udp.sock --eps 1000 --initial 555000000 --print-each 1000

Path: /tmp/udp.sock
Print each: 1 message(s)
Range: [0, 18446744073709551615]
Pattern: @{seq}
Events per second: 1000
Rampup (s): 0


Generating UDP messages...

<timestamp>                         <time(s)> <sequence>      <udp datagram>
___________________________________ _________ _______________ _______________________________
2023-08-02 19:16:36.340339 GMT      0         1               555000000
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
$> docker run -d --rm -it --name udp --entrypoint /opt/udp-server ghcr.io/testillano/h2agent:latest -k /tmp/udp.sock
$> docker exec -it udp /opt/udp-client -k /tmp/udp.sock # in foreground will throw client output
```

If the client is launched in background (-d) you won't be able to follow process output (`docker logs -f udp` shows server output because it was launched in first place).

In the **second case**, which is the recommended, we need to create an external volume:

```bash
$> docker volume create --name=socketVolume
```

And then, we can run the containers in separated shells (or both in background with '-d' because know they have independent docker logs):

```bash
$> docker run --rm -it -v socketVolume:/tmp --entrypoint /opt/udp-server ghcr.io/testillano/h2agent:latest -k /tmp/udp.sock
```

```bash
$> docker run --rm -it -v socketVolume:/tmp --entrypoint /opt/udp-client ghcr.io/testillano/h2agent:latest -k /tmp/udp.sock
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

`H2agent` server mock supports `SSL/TLS`. You may use helpers located under `tools/ssl` to create server key and certificate files:

```bash
$> ls tools/ssl/
create_all.sh  create_self-signed_certificate.sh
```

Using `create_all.sh`, server key and certificate are created at execution directory:

```bash
$> tools/ssl/create_all.sh
tools/ssl/create_all.sh
+ openssl genrsa -des3 -out ca.key 4096
Generating RSA private key, 4096 bit long modulus (2 primes)
..++++
..........++++
e is 65537 (0x010001)
Enter pass phrase for ca.key:
Verifying - Enter pass phrase for ca.key:
+ openssl req -new -x509 -days 365 -key ca.key -out ca.crt -subj '/C=ES/ST=Madrid/L=Madrid/O=Security/OU=IT Department/CN=www.example.com'
Enter pass phrase for ca.key:
+ openssl genrsa -des3 -out server.key 1024
Generating RSA private key, 1024 bit long modulus (2 primes)
...............................................+++++
.....................+++++

etc.
```

Add the following parameters to the agent command-line (appended key password to avoid the 'PEM pass phrase' prompt at process start):

```bash
--traffic-server-key server.key --traffic-server-crt server.crt --traffic-server-key-password <key password>
```

For quick testing, launch unsecured traffic in this way:

```bash
$> curl -i --http2-prior-knowledge --insecure -d'{"foo":1, "bar":2}' https://localhost:8000/any/unprovisioned/path
HTTP/2 501
```

**TODO**: support secure client connection for client capabilities.

## Metrics

Based in [prometheus data model](https://prometheus.io/docs/concepts/data_model/) and implemented with [prometheus-cpp library](https://github.com/jupp0r/prometheus-cpp), those metrics are collected and exposed through the server scraping port (`8080` by default, but configurable at [command line](#Command-line) by mean `--prometheus-port` option) and could be retrieved using Prometheus or compatible visualization software like [Grafana](https://prometheus.io/docs/visualization/grafana/) or just browsing `http://localhost:8080/metrics`.

More information about implemented metrics [here](#OAM).
To play with grafana automation in `h2agent` project, go to `./tools/grafana` directory and check its [PLAY_GRAFANA.md](./tools/grafana/PLAY_GRAFANA.md) file to learn more about.

## Traces and printouts

Traces are managed by `syslog` by default, but could be shown verbosely at standard output (`--verbose`) depending on the traces design level and the current level assigned. For example:

```bash
$> ./h2agent --verbose &
[1] 27407
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
$ ./h2a.sh --verbose # starts agent with docker by mean helper script
```

Or build native executable and run it from shell:

```bash
$> ./build-native.sh # builds executable
$> build/Release/bin/h2agent --verbose # starts executable
```

#### Working in training container

The training image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$> docker pull ghcr.io/testillano/h2agent_training:<tag>
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

A conversational bot is available in `./tools/questions-and-answers` directory. It is implemented in python using *langchain* and *OpenAI* (ChatGPT) technology. Check its [README.md](./tools/questions-and-answers/README.md) file to learn more about.

#### Play

A playground is available at `./tools/play-h2agent` directory. It is designed to guide through a set of easy examples. Check its [README.md](./tools/play-h2agent/README.md) file to learn more about.

#### Demo

A demo is available at `./demo` directory. It is designed to introduce the `h2agent` in a funny way with an easy use case. Open its [README.md](./demo/README.md) file to learn more about.

#### Kata

A kata is available at `./kata` directory. It is designed to guide through a set of exercises with increasing complexity. Check its [README.md](./kata/README.md) file to learn more about.

## Management interface

`h2agent` listens on a specific management port (*8074* by default) for incoming requests, implementing a *REST API* to manage the process operation. Through the *API* we could program the agent behavior. The following sections describe all the supported operations over *URI* path`/admin/v1/`.

We will start describing **general** mock operations:

* Schemas: define validation schemas used in further provisions to check the incoming and outgoing traffic.
* Global variables: shared variables between different provision contexts and flows. Normally not needed, but it is an extra feature to solve some situations by other means.
* Logging: dynamic logger configuration (update and check).
* General configuration (server).

Then, we will describe **traffic server mock** features:

* Server matching configuration: classification algorithms to  split the incoming traffic and access to the final procedure which will be applied.
* Server provision configuration: here we will define the mock behavior regarding the request received, and the transformations done over it to build the final response and evolve, if proceed, to another state for further receptions.
* Server data storage: data inspection is useful for both external queries (mainly troubleshooting) and internal ones (provision transformations). Also storage configuration will be described.

And finally, **traffic client mock** features:

* Client endpoints configuration: remote server addresses configured to be used by client provisions.
* Client provision configuration: here we will define the mock behavior regarding the request sent, and the transformations done over it to build the final request and evolve, if proceed, to another flow for further sendings.
* Client data storage: data inspection is useful for both external queries (mainly troubleshooting) and internal ones (provision transformations). Also storage configuration will be described.

## Management interface - general

### POST /admin/v1/schema

Loads schema(s) for future event validation. Added schemas could be referenced within provision configurations by mean their string identifier.

#### Request body schema

`POST` request must comply the following schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "id": {
      "type": "string"
    },
    "schema": {
      "type": "object"
    }
  },
  "required": [ "id", "schema" ]
}
```

If you have a `json` schema (from file `schema.json`) and want to build the `h2agent` schema configuration (into file `h2agent_schema.json`), you may perform automations like this *bash script* example:

```bash
$> jq --arg id "theSchemaId" '. | { id: $id, schema: . }' schema.json > h2agent_schema.json
```

Also *python* or any other language could do the job:

```python
>>> schema = {"$schema":"http://json-schema.org/draft-07/schema#","type":"object","additionalProperties":True,"properties":{"foo":{"type":"string"}},"required":["foo"]}
>>> print({ "id":"theSchemaId", "schema":schema })
```

##### **id**

Schema unique identifier. If the schema already exists, it will be overwritten.

##### **schema**

Content in `json` format to specify the schema definition.

#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### POST /admin/v1/schema (multiple schemas)

Load of a set of schemas through an array object is allowed. So, instead of launching *N* schema loads separately, you could group them as in the following example:

```json
[
  {
    "id": "myRequestsSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "foo": {
          "type": "string"
        }
      },
      "required": [
        "foo"
      ]
    }
  },
  {
    "id": "myResponsesSchema",
    "schema": {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "bar": {
          "type": "number"
        }
      },
      "required": [
        "bar"
      ]
    }
  }
]
```

Response status codes and body content follow same criteria than single load. A schema set fails with the first failed item, giving a 'pluralized' version of the single load failed response message.

### GET /admin/v1/schema/schema

Retrieves the schema of the schema operation body.

#### Response status code

**200** (OK).

#### Response body

Json object document containing the schema operation schema.

### GET /admin/v1/schema

Retrieves all the schemas configured.

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

Json array document containing all loaded items, when something is configured (no-content response has no body).

### DELETE /admin/v1/schema

Deletes all the process schemas loaded.

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

No response body.

### POST /admin/v1/global-variable

Global variables can be created dynamically from provisions execution (to be used there in later transformations steps or from any other different provision, due to the global scope), but they also can be loaded through this `REST API` operation. In any case, load operation is done appending provided data to the current one (in case that the variable already exists). This allows to use global variables as memory buckets, typical when they are managed from transformation steps (within provision context). But this operation is more focused on the use of global variables as constants for the whole execution (although they could be reloaded or reset from provisions, as commented, or even appended by other `POST` operations).

Global variables are created as string-value, which will be interpreted as numbers or any other data type, depending on the transformation involved.

#### Request body schema

`POST` request must comply the following schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "additionalProperties": false,
  "patternProperties": {
    "^.*$": {
      "anyOf": [
        {
          "type": "string"
        }
      ]
    }
  }
}
```

That is to say, and object with one level of fields with string value. For example:

```json
{
  "variable_name_1": "variable_value_1",
  "variable_name_2": "variable_value_2",
  "variable_name_3": "variable_value_3"
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

### GET /admin/v1/global-variable/schema

Retrieves the global variables schema.

#### Response status code

**200** (OK).

#### Response body

Json object document containing server data global schema.

### GET /admin/v1/global-variable?name=`<variable name>`

This operation retrieves the whole list of global variables created, or using the query parameter `name`, the specific variable selected:

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

When querying a specific variable, its value as string response body is retrieved:

```
<variable_value>
```

A variable used as memory bucket, could store even binary data and it may be obtained with this `REST API`operation.

When requesting the whole variables list, a Json object document with variable fields and their values (when something is stored, as no-content response has no body) is retrieved. Take the following `json` as an example of global list:

```json
{
  "variable_name_1": "variable_value_1",
  "variable_name_2": "variable_value_2",
  "variable_name_3": "variable_value_3"
}
```

### DELETE /admin/v1/global-variable?name=`<variable name>`

Deletes all the global variables registered or the selected one when query parameter is provided.

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

No response body.

### GET /admin/v1/files

This operation retrieves the whole list of files processed and their current status.

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

A `json` array with the list of files processed is retrieved. For example:

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

### GET /admin/v1/logging

Retrieves the current logging level of the `h2agent` process: `Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency`.

#### Response status code

**200** (OK).

#### Response body

String containing the current log level name.

### PUT /admin/v1/logging?level=`<level>`

Changes the log level of the `h2agent` process to any of the available levels (this can be also configured on start as described in [command line](#Command-line) section). So, `level` query parameter value could be any of the valid log levels: `Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency`.

#### Response status code

**200** (OK) or **400** (Bad Request).

### GET /admin/v1/configuration

Retrieve the general process configuration.

#### Response status code

**200** (OK)

#### Response body

For example:

```json
{
    "longTermFilesCloseDelayUsecs": 1000000,
    "shortTermFilesCloseDelayUsecs": 0,
    "lazyClientConnection": true
}
```

### PUT /admin/v1/server/configuration?receiveRequestBody=`<true|false>`&preReserveRequestBody=`<true|false>`

Request body reception can be disabled. This is useful to optimize the server processing in case that request body content is not actually needed by planned provisions. You could do this through the corresponding query parameter:

* `receiveRequestBody=true`: data received will be processed.
* `receiveRequestBody=false`: data received will be ignored.

The `h2agent` starts with request body reception enabled by default, but you could also disable this through command-line (`--traffic-server-ignore-request-body`).

Also, request body memory pre-reservation could be disabled to be dynamic. This simplifies the model behind (`http2comm` library) disabling the default optimization which minimizes reallocations done when data chunks are processed:

* `preReserveRequestBody=true`: pre reserves memory for the expected request body (with the maximum message size received in a given moment).
* `preReserveRequestBody=false`: allocates memory dynamically during append operations for data chunks processed.

The `h2agent` starts with memory pre reservation enabled by default, but you could also disable this through command-line (`--traffic-server-dynamic-request-body-allocation`).

#### Response status code

**200** (OK) or **400** (Bad Request).

### GET /admin/v1/server/configuration

Retrieve the general server configuration.

#### Response status code

**200** (OK)

#### Response body

For example:

```json
{
    "preReserveRequestBody": true,
    "receiveRequestBody": true
}
```

## Management interface - traffic server mock

### POST /admin/v1/server-matching

This the key piece for traffic classification towards server mock.

Defines the server matching procedure for incoming receptions on mock service. Every *URI* received is matched depending on the selected algorithm.
You can swap this algorithm safely keeping the existing provisions without side-effects, but normally, the mocked application should select an invariable matching configuration specially when long-term load testing is planned. For functional testing, as commented above, the matching configuration update is perfectly possible in real time.

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
        "enum": ["FullMatching", "FullMatchingRegexReplace", "RegexMatching"]
    },
    "rgx": {
      "type": "string"
    },
    "fmt": {
      "type": "string"
    },
    "uriPathQueryParameters": {
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "filter": {
          "type": "string",
            "enum": ["Sort", "PassBy", "Ignore"]
        },
        "separator": {
          "type": "string",
            "enum": ["Ampersand", "Semicolon"]
        }
      },
      "required": [ "filter" ]
    }
  },
  "required": [ "algorithm" ]
}
```

##### uriPathQueryParameters

Optional object used to specify the transformation used for traffic classification, of query parameters received in the *URI* path. It contains two fields, a mandatory _filter_ and an optional _separator_:

* *filter*:
  * *Sort*: this is the <u>default behavior</u>, which sorts, if received, query parameters to make provisions predictable for unordered inputs.
  * *PassBy*: if received, query parameters are used to classify without modifying the received *URI* path (query parameters are kept as received).
  * *Ignore*: if received, query parameters are ignored during classification (removed from *URI* path and not taken into account to match provisions). Note that query parameters are stored to be accessible on provision transformations, because this filter is only considered to classify traffic.
* *separator*:
  * *Ampersand*: this is the <u>default behavior</u> (if whole _uriPathQueryParameters_ object is not configured) and consists in split received query parameters keys using *ampersand* (`'&'`) as separator for key-value pairs.
  * *Semicolon*: using *semicolon* (`';'`) as query parameters pairs separator is rare but still applies on older systems.

##### rgx & fmt

Optional arguments used in `FullMatchingRegexReplace` algorithm.

##### algorithm

There are three classification algorithms. Two of them classify the traffic by single identification of the provision key (`method`, `uri` and `inState`): `FullMatching` matches directly, and `FullMatchingRegexReplace` matches directly after transformation. The other one, `RegexMatching` is not matching by identification but for regular expression.

Although we will explain them in detail, in summary we could consider those algorithms depending on the use cases tested:

* `FullMatching`: when <u>all</u> the *method/URIs* are completely <u>predictable</u>.

* `FullMatchingRegexReplace`: when <u>some</u> *URIs* should be <u>transformed</u> to get <u>predictable</u> ones (for example, timestamps trimming, variables in path or query parameters, etc.), and <u>other</u> *URIs* are disjoint with them, but also <u>predictable</u>.

* `RegexMatching`: when we have an <u>mix</u> of <u>unpredictable</u> *URIs* in our test plan.

###### FullMatching

Arguments `rgx`and `fmt` are not used here, so not allowed. The incoming request is fully translated into key without any manipulation, and then searched in internal provision map.

This is the default algorithm. Internal provision is stored in a map indexed with real requests information to compose an aggregated key (normally containing the requests *method* and *URI*, but as future proof, we could add `expected request` fingerprint). Then, when a request is received, the map key is calculated and retrieved directly to be processed.

This algorithm is very good and easy to use for predictable functional tests (as it is accurate), also giving internally  better performance for provision selection.

###### FullMatchingRegexReplace

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

###### RegexMatching

Arguments `rgx`and `fmt` are not used here, so not allowed. Provision keys are in this case, regular expressions to match reception keys. As we cannot search the real key in the provision map, we must check the reception sequentially against the list of regular expressions, and this is done assuming the first match as the valid one. So, this identification algorithm relies in the configured provision order to match the receptions and select the first valid occurrence.

This algorithm allows to provision with priority. For example, consider 3 provision operations which are provided sequentially in the following order:

1. `/ctrl/v2/id-55500[0-9]{4}/ts-[0-9]{10}`
2. `/ctrl/v2/id-5551122[0-9]{2}/ts-[0-9]{10}`
3. `/ctrl/v2/id-555112244/ts-[0-9]{10}`

If the `URI` "*/ctrl/v2/id-555112244/ts-1615562841*" is received, the second one is the first positive match and then, selected to mock the provisioned answer. Even being the third one more accurate, this algorithm establish an ordered priority to match the information.

Note: in case of large provisions, this algorithm could be not recommended (sequential iteration through provision keys is slower that map search performed in *full matching* procedures).

#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### GET /admin/v1/server-matching/schema

Retrieves the server matching schema.

#### Response status code

**200** (OK).

#### Response body

Json object document containing server matching schema.

### GET /admin/v1/server-matching

Retrieves the current server matching configuration.

#### Response status code

**200** (OK).

#### Response body

Json object document containing server matching configuration.

### POST /admin/v1/server-provision

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
        {"required": ["ConditionVar"]},
        {"required": ["EqualTo"]},
        {"required": ["DifferentFrom"]},
        {"required": ["JsonConstraint"]}
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
        "ConditionVar": { "type": "string", "pattern": "^!?.*$" },
        "EqualTo": { "type": "string" },
        "DifferentFrom": { "type": "string" },
        "JsonConstraint": { "type": "object" }
      }
    }
  },
  "type": "object",
  "additionalProperties": false,

  "properties": {
    "inState":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "outState":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "requestMethod": {
      "type": "string",
        "enum": ["POST", "GET", "PUT", "DELETE", "HEAD"]
    },
    "requestUri": {
      "type": "string"
    },
    "requestSchemaId": {
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
      "anyOf": [
        {"type": "object"},
        {"type": "array"},
        {"type": "string"},
        {"type": "integer"},
        {"type": "number"},
        {"type": "boolean"},
        {"type": "null"}
      ]
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
            "pattern": "^request\\.(uri(\\.(path$|param\\..+))?|body(\\..+)?|header\\..+)$|^response\\.body(\\..+)?$|^eraser$|^math\\..*|^random\\.[-+]{0,1}[0-9]+\\.[-+]{0,1}[0-9]+$|^randomset\\..+|^timestamp\\.[m|u|n]{0,1}s$|^strftime\\..+|^recvseq$|^(var|globalVar|serverEvent)\\..+|^(value)\\..*|^inState$|^txtFile\\..+|^binFile\\..+|^command\\..+"
          },
          "target": {
            "type": "string",
            "pattern": "^response\\.body\\.(string$|hexstring$)|^response\\.body\\.json\\.(object$|object\\..+|jsonstring$|jsonstring\\..+|string$|string\\..+|integer$|integer\\..+|unsigned$|unsigned\\..+|float$|float\\..+|boolean$|boolean\\..+)|^response\\.(header\\..+|statusCode|delayMs)$|^(var|globalVar|serverEvent)\\..+|^outState(\\.(POST|GET|PUT|DELETE|HEAD)(\\..+)?)?$|^txtFile\\..+|^binFile\\..+|^udpSocket\\..+|^break$"
          }
        },
        "additionalProperties" : {
          "$ref" : "#/definitions/filter"
        },
        "required": [ "source", "target" ]
      }
    },
    "responseSchemaId": {
      "type": "string"
    }
  },
  "required": [ "requestMethod", "responseCode" ]
}
```

##### inState and outState

We could label a provision specification to take advantage of internal *FSM* (finite state machine) for matched occurrences. When a reception matches a provision specification, the real context is searched internally to get the current state ("**initial**" if missing or empty string provided) and then get the  `inState` provision for that value. Then, the specific provision is processed and the new state will get the `outState` provided value. This makes possible to program complex flows which depends on some conditions, not only related to matching keys, but also consequence from [transformation filters](#Transform) which could manipulate those states.

These arguments are configured by default with the label "**initial**", used by the system when a reception does not match any internal occurrence (as the internal state is unassigned). This conforms a default rotation for further occurrences because the `outState` is again the next `inState`value. It is important to understand that if there is not at least 1 provision with `inState` = "**initial**" the matched occurrences won't never be processed. Also, if the next state configured (`outState` provisioned or transformed) has not a corresponding `inState` value, the flow will be broken/stopped.

So, "**initial**" is a reserved value which is mandatory to debut any kind of provisioned transaction. Remember that an empty string will be also converted to this special state for both `inState` and `outState` fields, and character `#` is not allowed (check [this](./docs/developers/AggregatedKeys.md) document for developers).

Important note:

Let's see an example to clarify:

* Provision *X* (match *m*, `inState`="*initial*"): `outState`="*second*", `response` *XX*
* Provision *Y* (match *m*, `inState`="*second*"): `outState`="*initial*", `response` *YY*
* Reception matches *m* and internal context map (server data) is empty: as we assume state "*initial*", we look for this  `inState` value for match *m*, which is provision *X*.
* Response *XX* is sent. Internal state will take the provision *X* `outState`, which is "*second*".
* Reception matches *m* and internal context map stores state "*second*", we look for this  `inState` value for match *m*, which is provision Y.
* Response *YY* is sent. Internal state will take the provision *Y* `outState`, which is "*initial*".

Further similar matches (*m*), will repeat the cycle again and again.

<u>Important note</u>: match *m* refers to matching key, that is to say: provision `method` and `uri`, but states are linked to real *URIs* received (coincide with match key `uri` for *FullMatching* classification algorithm, but not for others). So, there is a different state machine definition for each specific provision and so, a different current state for each specific events fulfilling such provision (this is much better that limiting the whole mock configuration with a global *FSM*, as for example, some events could fail due to *SUT* bugs and states would evolve different for their corresponding keys). If your mock receives several requests with different *URIs* for an specific test stage name, consider to name their provision states with the same identifier (with the stage name, for example), because different provisions will evolve at the "same time" and those names does not collide because they are different state machines (different matches). This could ease the flow understanding as those requests are received in a known test stage.

<u>Special **purge** state</u>: stateful scenarios normally require access to former events (available at server data storage) to evolve through different provisions, so disabling server data is not an option to make them work properly. The thing is that high load testing could impact on memory consumption of the mock server if we don't have a way to clean information which is no longer needed and could be dangerously accumulated. Here is where purge operation gets importance: the keyword '*purge*' is a reserved out-state used to indicate that server data related to an event history must be dropped (it should be configured at the last scenario stage provision). This mechanism is useful in long-term load tests to avoid the commented high memory consumption removing those scenarios which have been successfully completed. A nice side-effect of this design, is that all the failed scenarios will be available for further analysis, as purge operation is performed at last scenario stage and won't be reached normally in this case of fail.

##### requestMethod

Expected request method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*).

##### requestUri

Request *URI* path (percent-encoded) to match depending on the algorithm selected. It includes possible query parameters, depending on matching filters provided for them.

<u>*Empty string is accepted*</u>, and is reserved to configure an optional default provision, something which could be specially useful to define the fall back provision if no matching entry is found. So, you could configure defaults for each method, just putting an empty *request URI* or omitting this optional field. Default provisions could evolve through states (in/out) but at least "initial" is again mandatory to be processed.

##### requestSchemaId

We could optionally validate requests against a `json` schema. Schemas are identified by string name and configured through [command line](#Command-line) or [REST API](#Management-interface). When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

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

Response body. Currently supported: object (`json` and arrays), string, integer, number, boolean and null types.

##### responseDelayMs

Optional response delay simulation in milliseconds.

##### transform

Sorted list of transformation items to modify incoming information and build the dynamic response to be sent.

Each transformation has a `source`, a `target` and an optional `filter` algorithm. <u>The filters are applied over sources and sent to targets</u> (all the available filters at the moment act over sources in string format, so they need to be converted if they are not strings in origin).

A best effort is done to transform and convert information to final target vaults, and when something wrong happens, a logging error is thrown and the transformation filter is skipped going to the next one to be processed. For example, a source detected as *json* object cannot be assigned to a number or string target, but could be set into another *json* object.

Let's start describing the available sources of data: regardless the native or normal representation for every kind of target, the fact is that conversions may be done to almost every other type:

- *string* to *number* and *boolean* (true if non empty).

- *number* to *string* and *boolean* (true if different than zero).

- *boolean*: there is no source for boolean type, but you could create a non-empty string or non-zeroed number to represent *true* on a boolean target (only response body nodes could include a boolean).

- *json object*: request body node (whole document is indeed the root node) when being an object itself (note that it may be also a number or string). This data type can only be transfered into targets which support json objects like response body json node.



*Variables substitution:*

Before describing sources and targets (and filters), just to clarify that in some situations it is allowed the insertion of variables in the form `@{var id}` which will be replaced if exist, by local provision variables and global variables. In that case we will add the comment "**admits variables substitution**". At certain sources and targets, substitutions are not allowed because have no sense or they are rarely needed:



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

- response.body.`/<node1>/../<nodeN>`: response body node `json` path. This source path **admits variables substitution**. The use of provisioned response as template reference is rare but could ease the build of `json` structures for further transformations.

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

- var.`<id>`: general purpose variable (readable at transformation chain, provision-level scope). Cannot refer json objects. This source variable identifier **admits variables substitution**.

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
  $> tools/url.sh --encode "/app/v1/stock/madrid?loc=123&id=2"

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

- txtFile.`<path>`: reads text content from file with the path provided. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. Note that paths to missing files will fail to open. This source enables the `h2agent` capability to serve files.

- binFile.`<path>`: same as `txtFile` but reading binary data.

- command.`<command>`: executes command on process shell and captures the standard output/error ([popen](https://man7.org/linux/man-pages/man3/popen.3.html)() is used behind). Also, the return code is saved into provision local variable `rc`. You may call external scripts or executables, and do whatever needed as if you would be using the shell environment.

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

- var.`<id>` *[string (or number as string)]*: general purpose variable (writable at transformation chain and intended to be used later, as source, within provision-level scope). The idea of *variable* vaults is to optimize transformations when multiple transfers are going to be done (for example, complex operations like regular expression filters, are dumped to a variable, and then, we drop its value over many targets without having to repeat those complex algorithms again). Cannot store json objects. This target variable identifier **admits variables substitution**.

- globalVar.`<id>` *[string (or number as string)]*: general purpose global variable (writable at transformation chain and intended to be used later, as source, from anywhere as process-level scope). Cannot refer json objects. This target variable identifier **admits variables substitution**. <u>Target value is appended to the current existing value</u>. This allows to use global variables as memory buckets. So, you must use `eraser` to reset its value guaranteeing it starts from scratch.

- outState *[string (or number as string)]*: next processing state. This overrides the default provisioned one.

- outState.`[POST|GET|PUT|DELETE|HEAD][.<uri>]` *[string (or number as string)]*: next processing state for specific method (virtual server data will be created if needed: this way we could modify the flow for other methods different than the one which is managing the current provision). This target **admits variables substitution** in the `uri` part.

  You could, for example, simulate a database where a *DELETE* for an specific entry could infer through its provision an *out-state* for a foreign method like *GET*, so when getting that *URI* you could obtain a *404* (assumed this provision for the new *working-state* = *in-state* = *out-state* = "id-deleted"). By default, the same `uri` is used from the current event to the foreign method, but it could also be provided optionally giving more flexibility to generate virtual events with specific states.

- txtFile.`<path>` *[string]*: dumps source (as string) over text file with the path provided. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. Note that paths to missing directories will fail to open (the process does not create tree hierarchy). It is considered long term file (file is closed 1 second after last write, by default) when a constant path is configured, because this is normally used for specific log files. On the other hand, when any substitution may took place in the path provided (it has variables in the form `@{varname}`) it is considered as a dynamic name, so understood as short term file (file is opened, written and closed without delay, by default). **Note:** you can force short term type inserting a variable, for example with empty value: `txtFile./path/to/short-term-file.txt@{empty}`. Delays in microseconds are configurable on process startup. Check  [command line](#Command-line) for `--long-term-files-close-delay-usecs` and `--short-term-files-close-delay-usecs` options.

  This target can also be used to write named pipes (previously created: `mkfifo /tmp/mypipe && chmod 0666 /tmp/mypipe`), with the following restriction: writes must close the file descriptor everytime, so long/short term delays for close operations must be zero depending on which of them applies: variable paths zeroes the delay by default, but constant ones shall be zeroed too by command-line (`--long-term-files-close-delay-usecs 0`). Just like with regular UNIX pipes (`|`), when the writer closes, the pipe is torn down, so fast operations writting named pipes could provoke data looses (some writes missed). In that case, it is more recommended to use UDP unix sockets target (`udpSocket./tmp/udp.sock`).

- binFile.`<path>` *[string]*: same as `txtFile` but writting binary data.

- udpSocket.`<path>[|<milliseconds delay>]` *[string]*: sends source (as string) towards the UDP unix socket with the path provided, with an optional delay in milliseconds. The path can be relative (to the execution directory) or absolute, and **admits variables substitution**. UDP is a transport layer protocol in the TCP/IP suite, which provides a simple, connectionless, and unreliable communication service. It is a lightweight protocol that does not guarantee the delivery or order of data packets. Instead, it allows applications to send individual datagrams (data packets) to other hosts over the network without establishing a connection first. UDP is often used where low latency is crucial. In `h2agent` is useful to signal external applications to do associated tasks sharing specific data for the transactions processed. Use `./tools/udp-server` program to play with it or even better `./tools/udp-server-h2client` to generate HTTP/2 requests UDP-driven (this will be covered when full `h2agent` client capabilities are ready).

- serverEvent.`<server event address in query parameters format>`: this target is always used in conjunction with `eraser` source acting as an alternative purge method to the purge `outState`. The main difference is that states-driven purge method acts over processed events key (`method` and `uri` for the provision in which the purge state is planned), so not all the test scenarios may be covered with that constraint if they need to remove events registered for different transactions. In this case, event addressing is defined by request *method* (`requestMethod`), *URI* (`requestUri`), and events *number* (`eventNumber`): events number *path* (`eventPath`) is not accepted, as this operation just remove specific events or whole history, like REST API for server-data deletion:

  - *requestMethod*: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*). Mandatory.
  - *requestUri*: event *URI* selected. Mandatory.
  - *eventNumber*: position selected (*1..N*; *-1 for last*) within events list. Optional: if not provided, all the history may be purged.

  This target, as its source counterpart, **admits variables substitution**.

- break *[string]*: when non-empty string is transferred, the transformations list is interrupted. Empty string (or undefined source) ignores the action.



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
    "target": "response.body.json.string.category",
    "filter": { "RegexCapture" : "\/api\/v2\/id-[0-9]+\/category-([a-z]+)" }
  }
  ```

  In this example, it is not important to notice that we only have 1 captured group (we removed the brackets of the first one from the previous example). This is because the target is a path within the response body, not a variable, so, only the entire match (if proceed) will be transferred. Assuming we receive the same source from previous example, that value will be the entire *URI* path. If we would use a variable as target, such variable would store the same entire match, and also we would have *animal* as `<variable name>.1`.

  If you want to move directly the captured group (`animal`) to a non-variable target, you may use the next filter:



- RegexReplace: this is similar to the matching algorithm based in regular expressions and replace procedure (even the fact that it *falls back to source information when not matching is done*, something that differs from former `RegexCapture` algorithm which builds an empty string when regular expression is not fully matched). We provide `rgx` and `fmt` to transform the source into the target:

  ```json
  {
    "source": "request.uri.path",
    "target": "response.body.json.unsigned.data.timestamp",
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



- ConditionVar: conditional transfer from source to target based on the boolean interpretation of the string-value stored in the variable (both local and global variables are searched, giving priority to local ones), which is:

  - **False** condition for cases:
    - <u>Undefined</u> variable.
    - Defined but <u>empty</u> string.
  - **True** condition for the rest of cases:
  -  Defined variable with <u>non-empty</u> value: note that "0", "false" or any other "apparently false" non-empty string could be misinterpreted: they are absolutely true condition variables.

  Global variables are not inspected, only local ones.

  The adopted convention allows to use regular expression filters to **manually** create conditional variables, as non-matched sources skips target assignment (undefined is *false* condition) and matched ones copy the source (matched) into the target (variable) which will be a compliant condition variable (non-empty string is *true* condition):

  ```json
  {
    "source": "request.body./must/be/number",
    "target": "var.isNumber",
    "filter": { "RegexCapture" : "([0-9]+)" }
  }
  ```

  In summary, `isNumber` will be undefined if the request body node value at `/must/be/number` is not a number, and will hold that numeric value, so non-empty value, when it is actually a number (guaranteed by regular expression filter).

  Also, variable name in `ConditionVar` filter, can be preceded by <u>exclamation mark (!)</u>  in order to <u>invert the condition</u>:

  ```json
  {
    "source": "value.400",
    "target": "response.statusCode",
    "filter": { "ConditionVar" : "!isNumber" }
  },
  {
    "source": "value.id is empty",
    "target": "response.body.string",
    "filter": { "ConditionVar" : "!id" }
  }
  ```

  Condition variables may also be created **automatically** by some transformations into variable targets (condition variable), to be used later in this `ConditionVar` filter. The best example is the `JsonConstraint` filter (explained later) working together with variable target, as it outputs "1" when validation is successful and "" when fails.



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

  Note that `A_xor_B` could be also obtained with `(@{A}-@{B})^2` or `(@{A}+@{B})%2`.



- DifferentFrom: conditional transfer from source to target based in string comparison between the source and the provided value. This filter, **admits variables substitution**. Its use is similar to `EqualTo` and complement its logic in case we need to generate the negated variable.



- JsonConstraint: performs a `json` validation between the source (must be a valid document) and the provided filter `json` object.

  - If validation **succeed**, the string "1" is stored in selected target.
  - If validation **fails**, the validation report detail is stored in selected target. <u>If the target is a variable</u> (recommended), the validation report is stored in `<varname>.fail` variable, and `<varname>` will be emptied. So we could use `!<varname>` or `<varname>.fail` as equivalent condition variables.

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



Finally, after possible transformations, we could validate the response body:

##### responseSchemaId

We could optionally validate built responses against a `json` schema. Schemas are identified by string name and configured through [command line](#Command-line) or [REST API](#Management-interface). When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### POST /admin/v1/server-provision (multiple provisions)

Provision of a set of provisions through an array object is allowed. So, instead of launching *N* provisions separately, you could group them as in the following example:

```json
[
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/1",
    "responseCode": 200,
    "responseBody": {
      "foo": "bar-1"
    },
    "responseHeaders": {
      "content-type": "application/json",
      "x-version": "1.0.0"
    }
  },
  {
    "requestMethod": "GET",
    "requestUri": "/app/v1/foo/bar/2",
    "responseCode": 200,
    "responseBody": {
      "foo": "bar-2"
    },
    "responseHeaders": {
      "content-type": "application/json",
      "x-version": "1.0.0"
    }
  }
]
```

Response status codes and body content follow same criteria than single provisions. A provision set fails with the first failed item, giving a 'pluralized' version of the single provision failed response message although previous valid provisions will be added.

### GET /admin/v1/server-provision/schema

Retrieves the server provision schema.

#### Response status code

**200** (OK).

#### Response body

Json object document containing server provision schema.

### GET /admin/v1/server-provision

Retrieves all the provisions configured.

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

Json array document containing all provisioned items, when something is configured (no-content response has no body).

### DELETE /admin/v1/server-provision

Deletes the whole process provision. It is useful to clear the configuration if the provisioned data collides between different test cases and need to be reset.

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

No response body.

### PUT /admin/v1/server-data/configuration?discard=`<true|false>`&discardKeyHistory=`<true|false>`&disablePurge=`<true|false>`

There are three valid configurations for storage configuration behavior, depending on the query parameters provided:

* `discard=true&discardKeyHistory=true`: nothing is stored.
* `discard=false&discardKeyHistory=true`: no key history stored (only the last event for a key, except for unprovisioned events, which history is always respected for troubleshooting purposes).
* `discard=false&discardKeyHistory=false`: everything is stored: events and key history.

The combination `discard=true&discardKeyHistory=false` is incoherent, as it is not possible to store requests history with general events discarded. In this case, an status code *400 (Bad Request)* is returned.

The `h2agent` starts with full data storage enabled by default, but you could also disable this through command-line (`--discard-data` / `--discard-data-key-history`).

And regardless the previous combinations, you could enable or disable the purge execution when this reserved state is reached for a specific provision. Take into account that this stage has no sense if no data is stored but you could configure it anyway:

* `disablePurge=true`: provisions with `purge` state will ignore post-removal operation when this state is reached.
* `disablePurge=false`: provisions with `purge` state will process post-removal operation when this state is reached.

The `h2agent` starts with purge stage enabled by default, but you could also disable this through command-line (`--disable-purge`).

Be careful using this `PUT`operation in the middle of traffic load, because it could interfere and make unpredictable the server data information during tests. Indeed, some provisions with transformations based in event sources, could identify the requests within the history for an specific event assuming that a particular server data configuration is guaranteed.

#### Response status code

**200** (OK) or **400** (Bad Request).

### GET /admin/v1/server-data/configuration

Retrieve the server data configuration regarding storage behavior for general events and requests history.

#### Response status code

**200** (OK)

#### Response body

For example:

```json
{
    "purgeExecution": true,
    "storeEvents": true,
    "storeEventsKeyHistory": true
}
```

By default, the `h2agent` enables both kinds of storage types (general events and requests history events), and also enables the purge execution if any provision with this state is reached, so the previous response body will be returned on this query operation. This is useful for function/component testing where more information available is good to fulfill the validation requirements. In load testing, we could seize the `purge` out-state to control the memory consumption, or even disable storage flags in case that test plan is stateless and allows to do that simplification.

### GET /admin/v1/server-data?requestMethod=`<method>`&requestUri=`<uri>`&eventNumber=`<number>`&eventPath=`<path>`

Retrieves the current server internal data (requests received, their states and other useful information like timing or global order). Events received are stored <u>even if no provisions were found</u> for them (the agent answered with `501`, not implemented), being useful to troubleshoot possible configuration mistakes in the tests design. By default, the `h2agent` stores the whole history of events (for example requests received for the same `method` and `uri`) to allow advanced manipulation of further responses based on that information. <u>It is important to highlight that `uri` refers to the received `uri` normalized</u> (having for example, a better predictable query parameters order during server data events search), not the `classification uri` (which could dispense with query parameters or variable parts depending on the matching algorithm), and could also be slightly different in some cases (specially when query parameters are involved) than original `uri` received on HTTP/2 interface.

<u>Without query parameters</u> (`GET /admin/v1/server-data`), you may be careful with large contexts born from long-term tests (load testing), because a huge response could collapse the receiver (terminal or piped process). With query parameters, you could filter a specific entry providing *requestMethod*, *requestUri* and <u>optionally</u> a *eventNumber* and *eventPath*, for example:

`/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3&eventPath=/requestBody`

The `json` document response shall contain three main nodes: `method`, `uri` and a `events` object with the chronologically ordered list of events processed for the given `method/uri` combination.

Both *method* and *uri* shall be provided together (if any of them is missing, a bad request is obtained), and *eventNumber* cannot be provided alone as it is an additional filter which selects the history item for the `method/uri` key (the `events` node will contain a single register in this case). So, the *eventNumber* is the history position, **1..N** in chronological order, and **-1..-N** in reverse chronological order (latest one by mean -1 and so on). The zeroed value is not accepted. Also, *eventPath* has no sense alone and may be provided together with *eventNumber* because it refers to a path within the selected object for the specific position number described before.

This operation is useful for testing post verification stages (validate content and/or document schema for an specific interface). Remember that you could start the *h2agent* providing a requests schema file to validate incoming receptions through traffic interface, but external validation allows to apply different schemas (although this need depends on the application that you are mocking), and also permits to match the requests content that the agent received.

**Important note**: same thing must be considered about request *URI* encoding like in server event source definition: as this operation provides a list of query parameters, and one of these parameters is a *URI* itself (`requestUri`) it may be URL-encoded to avoid ambiguity with query parameters separators ('=', '&'). So, for the request *URI* `/app/v1/foo/bar/1?name=test` we would have (use `./tools/url.sh` helper to encode):

`/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&eventNumber=3&eventPath=/requestBody`

Once internally decoded, the request *URI* will be matched against the `uri` <u>normalized</u> as commented above, so encoding must be also done taking this normalization into account (query parameters order).

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

Json array document containing all the selected event items, when something matches (no-content response has no body).

When provided *method* and *uri*, server data will be filtered with that key. If event number is provided too, the single event object, if exists, will be returned. Same for event path (if nothing found, empty document is returned but status code will be 200, not 204). When no query parameters are provided, the whole internal data organized by key (*method* + *uri* ) together with their events arrays are returned.

Example of whole structure for a unique key (*GET* on '*/app/v1/foo/bar/1?name=test*'):

```json
[
  {
    "method": "GET",
    "events": [
      {
        "requestBody": {
          "node1": {
            "node2": "value-of-node1-node2"
          }
        },
        "requestHeaders": {
          "accept": "*/*",
          "category-id": "testing",
          "content-length": "52",
          "content-type": "application/x-www-form-urlencoded",
          "user-agent": "curl/7.58.0"
        },
        "previousState": "initial",
        "receptionTimestampUs": 1626047915716112,
        "responseBody": {
          "foo": "bar-1",
          "randomBetween10and30": 27
        },
        "responseDelayMs": 0,
        "responseHeaders": {
          "content-type": "text/html",
          "x-version": "1.0.0"
        },
        "responseStatusCode": 200,
        "serverSequence": 1,
        "state": "initial"
      },
      {
        "requestBody": {
          "node1": {
            "node2": "value-of-node1-node2"
          }
        },
        "requestHeaders": {
          "accept": "*/*",
          "category-id": "testing",
          "content-length": "52",
          "content-type": "application/x-www-form-urlencoded",
          "user-agent": "curl/7.58.0"
        },
        "previousState": "initial",
        "receptionTimestampUs": 1626047921641554,
        "responseBody": {
          "foo": "bar-1",
          "randomBetween10and30": 24
        },
        "responseDelayMs": 0,
        "responseHeaders": {
          "content-type": "text/html",
          "x-version": "1.0.0"
        },
        "responseStatusCode": 200,
        "serverSequence": 2,
        "state": "initial"
      }
    ],
    "uri": "/app/v1/foo/bar/1?name=test"
  }
]
```



Example of single event for a unique key (*GET* on '*/app/v1/foo/bar/1?name=test*') and a *eventNumber* (2):

```json
[
  {
    "method": "GET",
    "events": [
      {
        "requestBody": {
          "node1": {
            "node2": "value-of-node1-node2"
          }
        },
        "requestHeaders": {
          "accept": "*/*",
          "category-id": "testing",
          "content-length": "52",
          "content-type": "application/x-www-form-urlencoded",
          "user-agent": "curl/7.58.0"
        },
        "previousState": "initial",
        "receptionTimestampUs": 1626047921641684,
        "responseBody": {
          "foo": "bar-1",
          "randomBetween10and30": 24
        },
        "responseDelayMs": 0,
        "responseHeaders": {
          "content-type": "text/html",
          "x-version": "1.0.0"
        },
        "responseStatusCode": 200,
        "serverSequence": 2,
        "state": "initial"
      }
    ],
    "uri": "/app/v1/foo/bar/1?name=test"
  }
]
```

And finally an specific content within single event for unique key (*GET* on '*/app/v1/foo/bar/1?name=test*'), *eventNumber* (2) and a *eventPath* '*/requestBody*':

```json
{
  "node1": {
    "node2": "value-of-node1-node2"
  }
}
```



The information collected for a events item is:

* `virtualOrigin`: special field for virtual entries coming from provisions which established an *out-state* for a foreign method/uri. This entry is necessary to simulate complexes states but you should ignore from the post-verification point of view. The rest of *json* fields will be kept with the original event information, just in case the history is disabled, to allow tracking the maximum information possible. This node holds a `json` nested object containing the `method` and `uri` for the real event which generated this virtual register.
* `receptionTimestampUs`: event reception *timestamp*.
* `state`: working/current state for the event.
* `headers`: object containing the list of request headers.
* `body`: object containing the request body.
* `previousSate`: original provision state which managed this request.
* `responseBody`: response which was sent.
* `responseDelayMs`: delay which was processed.
* `responseStatusCode`: status code which was sent.
* `responseHeaders`: object containing the list of response headers which were sent.
* `serverSequence`: current server monotonically increased sequence for every reception (`1..N`). In case of a virtual register (if it contains the field `virtualOrigin`), this sequence is actually not increased for the server data entry shown, only for the original event which caused this one.

### GET /admin/v1/server-data/summary?maxKeys=`<number>`

When a huge amount of events are stored, we can still troubleshoot an specific known key by mean filtering the server data as commented in the previous section. But if we need just to check what's going on there (imagine a high amount of failed transactions, thus not purged), perhaps some hints like the total amount of receptions or some example keys may be useful to avoid performance impact in the process due to the unfiltered query, as well as difficult forensics of the big document obtained. So, the purpose of server data summary operation is try to guide the user to narrow and prepare an efficient query.

#### Response status code

**200** (OK).

#### Response body

A `json` object document with some practical information is built:

* `displayedKeys`: the summary could also be too big to be displayed, so query parameter *maxKeys* will limit the number (`amount`) of displayed keys in the whole response. Each key in the `list` is given by the *method* and *uri*, and also the number of history events (`amount`) is shown.
* `totalEvents`: this includes possible virtual events, although normally this kind of configuration is not usual and the value matches the total number of real receptions.
* `totalKeys`: total different keys (method/uri) registered.

Take the following `json` as an example:

```json
{
  "displayedKeys": {
    "amount": 3,
    "list": [
      {
        "amount": 2,
        "method": "GET",
        "uri": "/app/v1/foo/bar/1?name=test"
      },
      {
        "amount": 2,
        "method": "GET",
        "uri": "/app/v1/foo/bar/2?name=test"
      },
      {
        "amount": 2,
        "method": "GET",
        "uri": "/app/v1/foo/bar/3?name=test"
      }
    ]
  },
  "totalEvents": 45000,
  "totalKeys": 22500
}
```

### DELETE /admin/v1/server-data?requestMethod=`<method>`&requestUri=`<uri>`&eventNumber=`<number>`

Deletes the server data given by query parameters defined in the same way as former *GET* operation. For example:

`/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3`

Same restrictions apply here for deletion: query parameters could be omitted to remove everything, *method* and *URI* are provided together and *eventNumber* restricts optionally them.

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

No response body.

## Management interface - traffic client mock

### POST /admin/v1/client-endpoint

Defines client endpoint with the remote server information where `h2agent` may connect during test execution.

By default, created endpoints will connect the defined remote server (except for lazy connection mode: `--remote-servers-lazy-connection`) but no reconnection procedure is implemented in case of fail. Instead, they will be reconnected on demand when a request is processed through such endpoint.

#### Request body schema

`POST` request must comply the following schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",

  "type": "object",
  "additionalProperties": false,
  "properties": {
    "id": {
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "host": {
      "type": "string"
    },
    "port": {
      "type": "integer",
      "minimum": 0,
      "maximum": 65536
    },
    "secure": {
      "type": "boolean"
    },
    "permit": {
      "type": "boolean"
    }
  },
  "required": [ "id", "host", "port" ]
}
```

Mandatory fields are `id`, `host` and `port`. Optional `secure` field is used to indicate the scheme used, *http* (default) or *https*, and `permit` field is used to process (default) or ignore a request through the client endpoint regardless if the connection is established or not (when permitted, a closed connection will be lazily restarted). Using `permit`, flows may be interrupted without having to disconnect the carrier.

Endpoints could be updated through further *POST* requests to the same identifier `id`. When `host`, `port` and/or `secure` are modified for an existing endpoint, connection shall be dropped and re-created again towards the corresponding updated address. In this case, status code *Accepted* (202) will be returned.

#### Response status code

**201** (Created), **202** (Accepted) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### POST /admin/v1/client-endpoint (multiple client endpoints)

Configuration of a set of client endpoints through an array object is allowed. So, instead of launching *N* configurations separately, you could group them as in the following example:

```json
[
  {
    "id": "myServer1",
    "host": "localhost1",
    "port": 8000
  },
  {
    "id": "myServer2",
    "host": "localhost2",
    "port": 8000
  }
]
```

Response status codes and body content follow same criteria than single configurations. A client endpoint set fails with the first failed item, giving a 'pluralized' version of the single configuration failed response message although previous valid client endpoints will be added.

### GET /admin/v1/client-endpoint/schema

Retrieves the client endpoint schema.

#### Response status code

**200** (OK).

#### Response body

Json object document containing client endpoint schema.

### GET /admin/v1/client-endpoint

Retrieves the current client endpoint configuration. An additional `status` field will be answered in the response object for every client endpoint indicating the current connection status.

#### Response status code

**200** (OK), **204** (No Content).

#### Response body

Json object document containing client endpoint configuration. No content has no body.

### DELETE /admin/v1/client-endpoint

Deletes the whole process client endpoint configuration. All the established connections will be closed and client endpoints will be removed from the list.

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

No response body.

### POST /admin/v1/client-provision

Client provisions are a fundamental part of the client mode configuration. Unlike server provisions, they are identified by the mandatory `id` identifier (in server mode, the primary identifier was the `method/uri` key) and the optional `inState` field (which defaults to "initial" when missing). In the client mode, there are no classification algorithms because the provisions are actively triggered through the *REST API*. In client mode, the meaning of `inState` is slightly different and represents the evolution for a given <u>identifier understood as specific test scenario</u>: the state shall transition for each of its stages (`outState` dictates the next provision key to be processed). The rest of the fields, defined by the `json` schema below, are self-explanatory, namely: request body and headers, delay before sending the configured request, allowable timeout to get response, endpoint to connect (which `id` was configured in previous *REST API* section: "client-endpoint"), request and response schemes to validate, etc.

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
        {"required": ["ConditionVar"]},
        {"required": ["EqualTo"]},
        {"required": ["DifferentFrom"]},
        {"required": ["JsonConstraint"]}
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
        "ConditionVar": { "type": "string", "pattern": "^!?.*$" },
        "EqualTo": { "type": "string" },
        "DifferentFrom": { "type": "string" },
        "JsonConstraint": { "type": "object" }
      }
    }
  },
  "type": "object",
  "additionalProperties": false,

  "properties": {
    "id":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "inState":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "outState":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "endpoint":{
      "type": "string",
      "pattern": "^[^#]*$"
    },
    "requestMethod": {
      "type": "string",
        "enum": ["POST", "GET", "PUT", "DELETE", "HEAD" ]
    },
    "requestUri": {
      "type": "string"
    },
    "requestSchemaId": {
      "type": "string"
    },
    "requestHeaders": {
      "additionalProperties": {
        "type": "string"
       },
       "type": "object"
    },
    "requestBody": {
      "anyOf": [
        {"type": "object"},
        {"type": "array"},
        {"type": "string"},
        {"type": "integer"},
        {"type": "number"},
        {"type": "boolean"},
        {"type": "null"}
      ]
    },
    "requestDelayMs": {
      "type": "integer"
    },
    "timeoutMs": {
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
            "pattern": "^request\\.(uri|body(\\..+)?|header\\..+)$|^eraser$|^math\\..*|^random\\.[-+]{0,1}[0-9]+\\.[-+]{0,1}[0-9]+$|^randomset\\..+|^timestamp\\.[m|u|n]{0,1}s$|^strftime\\..+|^sendseq$|^seq$|^(var|globalVar|clientEvent)\\..+|^(value)\\..*|^inState$|^txtFile\\..+|^binFile\\..+|^command\\..+"
          },
          "target": {
            "type": "string",
            "pattern": "^request\\.body\\.(string$|hexstring$)|^request\\.body\\.json\\.(object$|object\\..+|jsonstring$|jsonstring\\..+|string$|string\\..+|integer$|integer\\..+|unsigned$|unsigned\\..+|float$|float\\..+|boolean$|boolean\\..+)|^request\\.(header\\..+|delayMs|timeoutMs)$|^(var|globalVar|clientEvent)\\..+|^outState$|^txtFile\\..+|^binFile\\..+|^udpSocket\\..+"
          }
        },
        "additionalProperties" : {
          "$ref" : "#/definitions/filter"
        },
        "required": [ "source", "target" ]
      }
    },
    "onResponseTransform" : {
      "type" : "array",
      "minItems": 1,
      "items" : {
        "type" : "object",
        "minProperties": 2,
        "maxProperties": 3,
        "properties": {
          "source": {
            "type": "string",
            "pattern": "^request\\.(uri(\\.(path$|param\\..+))?|body(\\..+)?|header\\..+)$|^response\\.(body(\\..+)?|header\\..+|statusCode)$|^eraser$|^math\\..*|^random\\.[-+]{0,1}[0-9]+\\.[-+]{0,1}[0-9]+$|^randomset\\..+|^timestamp\\.[m|u|n]{0,1}s$|^strftime\\..+|^sendseq$|^seq$|^(var|globalVar|clientEvent)\\..+|^(value)\\..*|^inState$|^txtFile\\..+|^binFile\\..+|^command\\..+"
          },
          "target": {
            "type": "string",
            "pattern": "^(var|globalVar|clientEvent)\\..+|^outState$|^txtFile\\..+|^binFile\\..+|^udpSocket\\..+|^break$"
          }
        },
        "additionalProperties" : {
          "$ref" : "#/definitions/filter"
        },
        "required": [ "source", "target" ]
      }
    },
    "responseSchemaId": {
      "type": "string"
    }
  },
  "required": [ "id" ]
}
```

**id**

Client provision identifier.

##### inState and outState

As we mentioned above, states here represents scenario stages:

Let's see an example to clarify:

* `id`="*scenario1*", `inState`="*initial*", `outState`="*second*"
* `id`="*scenario1*", `inState`="*second*", `outState`="*third*"
* `id`="*scenario1*", `inState`="*third*", `outState`="*purge*"

When *scenario1* is triggered, its current state is searched assuming "initial" when nothing is found in client data storage. So it will be processed and next stage is triggered automatically for the new combination `id` + `outState` when the response is received (timeout is a kind of response but normally user stops the scenario in this case). System test is possible because those stages are replicated by mean different instances of the same scenario evolving separately: this is driven by an internal sequence identifier which is used to calculate real request *method* and *uri*, the ones stored in the data base (this mechanism will be deeply explained later).

The `outState` holds a reserved default value of `road-closed` for any provision when it is not explicitly configured. This is because here, the provision is not reset and must be guided by the flow execution. This `outState` can be configured on request transformation before sending and after response is received so new flows can be triggered with different stages, but they are unset by default (`road-closed`). This special value is not accepted for `inState` field to guarantee its reserved meaning.

<u>Special **purge** state</u>: stateful scenarios normally require access to former events (available at client data storage) to evolve through different provisions, so disabling client data is not an option to make them work properly. The thing is that high load testing could impact on memory consumption of the mock server if we don't have a way to clean information which is no longer needed and could be dangerously accumulated. Here is where purge operation gets importance: the keyword '*purge*' is a reserved out-state used to indicate that client data related to an scenario (everything for a given `id` and internal sequence) history must be dropped (it should be configured at the last scenario stage provision). This mechanism is useful in long-term load tests to avoid the commented high memory consumption removing those scenarios which have been successfully completed. A nice side-effect of this design, is that all the failed scenarios will be available for further analysis, as purge operation is performed at last scenario stage and won't be reached normally in this case of fail.

**endpoint**

Client endpoint identifier.

##### requestMethod

Expected request method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*).

It can be omitted in the provision, but it is mandatory to be available (so it should be created on transformations) when preparing the request to be sent.

##### requestUri

Request *URI* path (percent-encoded). It includes possible query parameters to be replaced during transformations (previous to request sending).

This is normally completed/appended by dynamic sequences in order to configure the final *URI* to be sent (variables or filters can be used to build that *URI*). So, transformation list may built a request *URI* different than provision template value, which will be the one to send and optionally register in client data storage events.

It can be omitted in the provision, but it is mandatory to be available (so it should be created on transformations) when preparing the request to be sent.

##### requestSchemaId

We could optionally validate built request (after transformations) against a `json` schema. Schemas are identified by string name and configured through [command line](#Command-line) or [REST API](#Management-interface). When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

##### requestHeaders

Header fields for the request. For example:

```json
"requestHeaders":
{
  "content-type": "application/json"
}
```

##### requestBody

Request body. Currently supported: object (`json` and arrays), string, integer, number, boolean and null types.

##### requestDelayMs

Optional request delay simulation in milliseconds.

##### timeoutMs

Optional timeout for response in milliseconds.

##### transform & onTransformResponse

As in the server mode, we have transformations to be applied, but this time we can transform the context before sending (**onTransform** node), and when the response is received (**onTransformReponse** node).

Items are already known. Most of them are described in the server mock section (server-provision). Here, work in the same way, but there are few new ones: sources *sendseq* and *seq*, targets *request.delayMs*, *request.timeoutMs* and *break*. The `outState` does not support foreign states, and request/response bodies here are swapped with server mode variants (request template is accessed as source and target on request transformation and source on response transformation, and response is accessed as source on response transformation).

New **sources**:

- sendseq: sequence id number increased for every mock sending over specific client endpoint (starts on *1* when the *h2agent* is started).

- seq: sequence id number provided by client provision trigger procedure (we will explain later, the ways to generate a unique value or full range with given rate). This value is accessible for every provision processing and is used to create dynamically things like the final request *URI* sent (containing for example, a session identifier) and probably some parts of the request body content.


New **targets**:

- request.delayMs *[unsigned integer]*: simulated delay before sending the request: although you can configure a fixed value for this property on provision document, this transformation target overrides it.
- request.timeoutMs *[unsigned integer]*: timeout to wait for the response: although you can configure a fixed value for this property on provision document, this transformation target overrides it.
- break: this target is activated with non-empty source (for example `value.1`) and interrupts the transformation list. It is used on response context to discard further transformations when, for example, response status code is not valid to continue processing the test scenario. Normally, we should "dirty" the `outState` (for example, setting an unprovisioned "road closed" state, in order to stop the flow) and then break the transformation procedure (this also dodges a probable purge state configured in next stages, keeping internal data for further analysis).

##### responseSchemaId

We could optionally validate received responses against a `json` schema. Schemas are identified by string name and configured through [command line](#Command-line) or [REST API](#Management-interface). When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

#### Response status code

**201** (Created) or **400** (Bad Request).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### POST /admin/v1/client-provision (multiple provisions)

Provision of a set of provisions through an array object is allowed. So, instead of launching *N* provisions separately, you could group them as in the following example:

```json
[
  {
    "id": "test1",
    "endpoint": "myClientEndpoint",
    "requestMethod": "POST",
    "requestUri": "/app/v1/stock/madrid?loc=123",
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
    "requestDelayMs": 20,
    "timeoutMs": 2000
  },
  {
    "id": "test2",
    "endpoint": "myClientEndpoint2",
    "requestMethod": "POST",
    "requestUri": "/app/v1/stock/malaga?loc=124",
    "requestBody": {
      "engine": "hdi",
      "model": "peugeot",
      "year": 2023
    },
    "requestHeaders": {
      "accept": "*/*",
      "content-length": "52",
      "content-type": "application/x-www-form-urlencoded",
      "user-agent": "curl/7.77.0"
    },
    "requestDelayMs": 20,
    "timeoutMs": 2000
  }
]
```

Response status codes and body content follow same criteria than single provisions. A provision set fails with the first failed item, giving a 'pluralized' version of the single provision failed response message although previous valid provisions will be added.

### GET /admin/v1/client-provision/schema

Retrieves the client provision schema.

#### Response status code

**200** (OK).

#### Response body

Json object document containing client provision schema.

### GET /admin/v1/client-provision

Retrieves all the provisions configured.

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

Json array document containing all provisioned items, when something is configured (no-content response has no body).

### DELETE /admin/v1/client-provision

Deletes the whole process provision. It is useful to clear the configuration if the provisioned data collides between different test cases and need to be reset.

#### Response status code

**200** (OK), **202** (Accepted) or **204** (No Content).

#### Response body

No response body.

### GET /admin/v1/client-provision/`<id>`?inState=`<inState>`&sequenceBegin=`<number>`&sequenceEnd=`<number>`&rps=`<number>`&repeat=`<true|false>` (triggering)

To trigger a client provision, we will use the *GET* method, providing its identifier in the *URI*.

<u>**Work in progress for the following information (at the moment, only single request is implemented, so only functional testing may be driven):**</u>

Normally we shall trigger only provisions for `inState` = "initial" (so, it is the default value when this query parameter is missing). This is because the traffic flow will evolve activating other provision keys given by the <u>same</u> provision identifier but another `inState`. All those internal triggers are indirectly caused by the primal administrative operation which is the only one externally initiated. Although it is possible to trigger an intermediate state, that is probably for debugging purposes.

Also, optional query parameters can be specified to perform multiple triggering (status code *202* is used in operation response instead of *200* used for single request sending). This operation creates internal events sequenced in a range of values (`sequence` variable will be available in provision process for each iterated value) and with specific rate (events per second) to perform system/load tests.

Each client provision can evolve the range of values independently of others, and triggering process may be stopped (with `rps` zero-valued) and then resumed again with a positive rate. Also repeat mode is stored as part of provision trigger configuration with these defaults: range `[0, 0]`, rate of '0' and repeat 'false'.

*Query parameters:*

* `sequenceBegin`: initial `sequence` variable (non-negative value).
* `sequenceEnd`: final `sequence` variable (non-negative value).
* `rps`: rate in requests per second triggered (non-negative value, '0' to stop).
* `repeat`: range repetition once exhausted (true or false).

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

#### Response status code

**200** (OK), **202** (Accepted), **400** (Bad Request) or **404** (Not Found).

#### Response body

```json
{
  "result":"<true or false>",
  "response":"<additional information>"
}
```

### PUT /admin/v1/client-data/configuration?discard=`<true|false>`&discardKeyHistory=`<true|false>`&disablePurge=`<true|false>`

Same explanation done for `server-data` equivalent operation, applies here. Just to know that history events here have a extended key adding `client endpoint id` to the `method` and `uri` processed. The purge procedure is performed over the specific provision identifier, removing everything registered for any working `state` and for the current processed `sequence` value.

The same agent could manage server and client connections, so you have specific configurations for internal data regarding server or client events, but normally, we shall use only one mode to better separate responsibilities within the testing ecosystem.

#### Response status code

**200** (OK) or **400** (Bad Request).

### GET /admin/v1/client-data/configuration

Retrieve the client data configuration regarding storage behavior for general events and requests history.

#### Response status code

**200** (OK)

#### Response body

For example:

```json
{
    "purgeExecution": true,
    "storeEvents": true,
    "storeEventsKeyHistory": true
}
```

By default, the `h2agent` enables both kinds of storage types (general events and requests history events), and also enables the purge execution if any provision with this state is reached, so the previous response body will be returned on this query operation. This is useful for function/component testing where more information available is good to fulfill the validation requirements. In load testing, we could seize the `purge` out-state to control the memory consumption, or even disable storage flags in case that test plan is stateless and allows to do that simplification.

### GET /admin/v1/client-data?clientEndpointId=`<ceid>`&requestMethod=`<method>`&requestUri=`<uri>`&eventNumber=`<number>`&eventPath=`<path>`

Retrieves the current client internal data (requests sent, their provision identifiers, states and other useful information like timing or global order). By default, the `h2agent` stores the whole history of events (for example requests sent for the same `clientEndpointId`, `method` and `uri`) to allow advanced manipulation of further responses based on that information. <u>It is important to highlight that `uri` refers to the final sent `uri` normalized</u> (having for example, a better predictable query parameters order during client data events search), not necessarily the `provisioned uri` within the provision template.

<u>Without query parameters</u> (`GET /admin/v1/client-data`), you may be careful with large contexts born from long-term tests (load testing), because a huge response could collapse the receiver (terminal or piped process). With query parameters, you could filter a specific entry providing *clientEndpointId*, *requestMethod*, *requestUri* and <u>optionally</u> a *eventNumber* and *eventPath*, for example:

`/admin/v1/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3&eventPath=/responseBody`

The `json` document response shall contain three main nodes: `clientEndpointId`, `method`, `uri` and a `events` object with the chronologically ordered list of events processed for the given `clientEndpointId/method/uri` combination.

Both *clientEndpointId*, *method* and *uri* shall be provided together (if any of them is missing, a bad request is obtained), and *eventNumber* cannot be provided alone as it is an additional filter which selects the history item for the `clientEndpointId/method/uri` key (the `events` node will contain a single register in this case). So, the *eventNumber* is the history position, **1..N** in chronological order, and **-1..-N** in reverse chronological order (latest one by mean -1 and so on). The zeroed value is not accepted. Also, *eventPath* has no sense alone and may be provided together with *eventNumber* because it refers to a path within the selected object for the specific position number described before.

This operation is useful for testing post verification stages (validate content and/or document schema for an specific interface). Remember that you could start the *h2agent* providing a response schema file to validate incoming responses through traffic interface, but external validation allows to apply different schemas (although this need depends on the application that you are mocking).

**Important note**: same thing must be considered about request *URI* encoding like in client event source definition: as this operation provides a list of query parameters, and one of these parameters is a *URI* itself (`requestUri`) it may be URL-encoded to avoid ambiguity with query parameters separators ('=', '&'). So, for the request *URI* `/app/v1/foo/bar/1?name=test` we would have (use `./tools/url.sh` helper to encode):

`/admin/v1/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar%3Fid%3D5%26name%3Dtest&eventNumber=3&eventPath=/responseBody`

Once internally decoded, the request *URI* will be matched against the `uri` <u>normalized</u> as commented above, so encoding must be also done taking this normalization into account (query parameters order).

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

Json array document containing all the selected event items, when something matches (no-content response has no body).

When provided *clientEndpointId*, *method* and *uri*, client data will be filtered with that key. If event number is provided too, the single event object, if exists, will be returned. Same for event path (if nothing found, empty document is returned but status code will be 200, not 204). When no query parameters are provided, the whole internal data organized by key (*clientEndpointId* + *method* + *uri* ) together with their events arrays are returned.

Example of whole structure for a unique key (*POST* on '*/app/v1/stock/madrid?loc=123*' for *'myClientProvision'*):

```json
[
  {
    "clientEndpointId": "myClientEndpoint",
    "events": [
      {
        "clientProvisionId": "test",
        "clientSequence": 1,
        "previousState": "initial",
        "receptionTimestampUs": 1685404454368627,
        "requestBody": {
          "engine": "tdi",
          "model": "audi",
          "year": 2021
        },
        "requestDelayMs": 20,
        "requestHeaders": {
          "content-type": "application/json",
          "user-agent": "curl/7.77.0"
        },
        "responseBody": {
          "bar": 2,
          "foo": 1
        },
        "responseHeaders": {
          "content-type": "application/json",
          "date": "Mon, 29 May 2023 23:54:14 GMT"
        },
        "responseStatusCode": 200,
        "sendingTimestampUs": 1685404454368448,
        "sequence": 0,
        "state": "road-closed",
        "timeoutMs": 2000
      },
      {
        "clientProvisionId": "test",
        "clientSequence": 2,
        "previousState": "initial",
        "receptionTimestampUs": 1685404456238974,
        "requestBody": {
          "engine": "tdi",
          "model": "audi",
          "year": 2021
        },
        "requestDelayMs": 20,
        "requestHeaders": {
          "content-type": "application/json",
          "user-agent": "curl/7.77.0"
        },
        "responseBody": {
          "bar": 2,
          "foo": 1
        },
        "responseHeaders": {
          "content-type": "application/json",
          "date": "Mon, 29 May 2023 23:54:16 GMT"
        },
        "responseStatusCode": 200,
        "sendingTimestampUs": 1685404456238760,
        "sequence": 0,
        "state": "road-closed",
        "timeoutMs": 2000
      }
    ],
    "method": "POST",
    "uri": "/app/v1/stock/madrid?loc=123"
  }
]
```


Example of single event for a unique key (*POST* on '*/app/v1/stock/madrid?loc=123*') and a *eventNumber* (2):

```json
{
  "clientProvisionId": "test",
  "clientSequence": 2,
  "previousState": "initial",
  "receptionTimestampUs": 1685404456238974,
  "requestBody": {
    "engine": "tdi",
    "model": "audi",
    "year": 2021
  },
  "requestDelayMs": 20,
  "requestHeaders": {
    "content-type": "application/json",
    "user-agent": "curl/7.77.0"
  },
  "responseBody": {
    "bar": 2,
    "foo": 1
  },
  "responseHeaders": {
    "content-type": "application/json",
    "date": "Mon, 29 May 2023 23:54:16 GMT"
  },
  "responseStatusCode": 200,
  "sendingTimestampUs": 1685404456238760,
  "sequence": 0,
  "state": "road-closed",
  "timeoutMs": 2000
}
```

And finally an specific content within single event for unique key (*POST* on '*/app/v1/stock/madrid?loc=123*'), *eventNumber* (2) and a *eventPath* '*/responseBody*':

```json
{
  "bar": 2,
  "foo": 1
}
```



The information collected for a events item is:

* `clientProvisionId`: provision identifier.
* `clientSequence`: current client monotonically increased sequence for every sending (`1..N`).
* `sendingTimestampUs`: event sending *timestamp* (request).
* `receptionTimestampUs`: event reception *timestamp* (response).
* `state`: working/current state for the event.
* `requestHeaders`: object containing the list of request headers.
* `requestBody`: object containing the request body.
* `previousSate`: original provision state which managed this request.
* `responseBody`: response which was received.
* `requestDelayMs`: delay for outgoing request.
* `responseStatusCode`: status code which was received.
* `responseHeaders`: object containing the list of response headers which were received.
* `sequence`: internal provision sequence.
* `timeoutMs`: accepted timeout for request response.

### GET /admin/v1/client-data/summary?maxKeys=`<number>`

When a huge amount of events are stored, we can still troubleshoot an specific known key by mean filtering the client data as commented in the previous section. But if we need just to check what's going on there (imagine a high amount of failed transactions, thus not purged), perhaps some hints like the total amount of sendings or some example keys may be useful to avoid performance impact in the process due to the unfiltered query, as well as difficult forensics of the big document obtained. So, the purpose of client data summary operation is try to guide the user to narrow and prepare an efficient query.

#### Response status code

**200** (OK).

#### Response body

A `json` object document with some practical information is built:

* `displayedKeys`: the summary could also be too big to be displayed, so query parameter *maxKeys* will limit the number (`amount`) of displayed keys in the whole response. Each key in the `list` is given by the *clientEndpointId*, *method* and *uri*, and also the number of history events (`amount`) is shown.
* `totalEvents`: total number of events.
* `totalKeys`: total different keys (clientEndpointId/method/uri) registered.

Take the following `json` as an example:

```json
{
  "displayedKeys": {
    "amount": 3,
    "list": [
      {
        "amount": 2,
        "clientEndpointId": "myClientEndpointId",
        "method": "GET",
        "uri": "/app/v1/foo/bar/1?name=test"
      },
      {
        "amount": 2,
        "clientEndpointId": "myClientEndpointId",
        "method": "GET",
        "uri": "/app/v1/foo/bar/2?name=test"
      },
      {
        "amount": 2,
        "clientEndpointId": "myClientEndpointId",
        "method": "GET",
        "uri": "/app/v1/foo/bar/3?name=test"
      }
    ]
  },
  "totalEvents": 45000,
  "totalKeys": 22500
}
```

### DELETE /admin/v1/client-data?clientEndpointId=`<ceid>`&requestMethod=`<method>`&requestUri=`<uri>`&eventNumber=`<number>`

Deletes the client data given by query parameters defined in the same way as former *GET* operation. For example:

`/admin/v1/client-data?clientEndpointId=myClientEndpointId&requestMethod=GET&requestUri=/app/v1/foo/bar/5&eventNumber=3`

Same restrictions apply here for deletion: query parameters could be omitted to remove everything, *clientEndpointId*, *method* and *URI* are provided together and *eventNumber* restricts optionally them.

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

No response body.

## How it is delivered

`h2agent` is delivered in a `helm` chart called `h2agent` (`./helm/h2agent`) so you may integrate it in your regular `helm` chart deployments by just adding a few artifacts.
This chart deploys the `h2agent` pod based on the docker image with the executable under `./opt` together with some helper functions to be sourced on docker shell: `/opt/utils/helpers.src` (default directory path can be modified through `utilsMountPath` helm chart value).
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

As we commented [above](#How-it-is-delivered), the `h2agent` helm chart packages a helper functions script which is very useful for troubleshooting. This script is also available for native usage (`./tools/helpers.src`):

```bash
$> source ./tools/helpers.src

===== h2agent operation helpers =====
Shortcut helpers (sourced variables and functions)
to ease agent operation over management interface:
   https://github.com/testillano/h2agent#management-interface

=== Variables ===
TRAFFIC_URL=http://localhost:8000
ADMIN_URL=http://localhost:8074/admin/v1
CURL="curl -i --http2-prior-knowledge"

=== General ===
Usage: schema [-h|--help] [--clean] [file]; Cleans/gets/updates current schema configuration
                                            (http://localhost:8074/admin/v1/schema).
Usage: global_variable [-h|--help] [--clean] [name|file]; Cleans/gets/updates current agent global variable configuration
                                                          (http://localhost:8074/admin/v1/global-variable).
Usage: files [-h|--help]; Gets the files processed.
Usage: files_configuration [-h|--help]; Manages files configuration (gets current status by default).
                            [--enable-read-cache]  ; Enables cache for read operations.
                            [--disable-read-cache] ; Disables cache for read operations.
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

Usage: server_matching [-h|--help]; Gets/updates current server matching configuration
                                    (http://localhost:8074/admin/v1/server-matching).
Usage: server_provision [-h|--help] [--clean] [file]; Cleans/gets/updates current server provision configuration
                                                      (http://localhost:8074/admin/v1/server-provision).
Usage: server_data [-h|--help]; Inspects server data events (http://localhost:8074/admin/v1/server-data).
                   [method] [uri] [[-]event number] [event path] ; Restricts shown data with given positional filters.
                                                                   Event number may be negative to access by reverse
                                                                   chronological order.
                   [--summary] [max keys]          ; Gets current server data summary to guide further queries.
                                                     Displayed keys (method/uri) could be limited (5 by default, -1: no limit).
                   [--clean] [query filters]       ; Removes server data events. Admits additional query filters to narrow the
                                                     selection.
Usage: server_data_sequence [-h|--help] [value (available values by default)]; Extract server sequence document from json
                                                                               retrieved in previous server_data() call.

=== Traffic client ===
Usage: client_endpoint [-h|--help] [--clean] [file]; Cleans/gets/updates current client endpoint configuration
                                                     (http://localhost:8074/admin/v1/client-endpoint).

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

=== Auxiliary ===
Usage: json [-h|--help]; Beautifies previous operation json response content.
            [jq expression, '.' by default]; jq filter over previous content.
            Example filter: schema && json '.[] | select(.id=="myRequestsSchema")'
            Auto-execution: assign non-empty value to 'BEAUTIFY_JSON'.
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
$> kubectl exec -it -n ns-ct-h2agent h2agent-55b9bd8d4d-2hj9z -- sh -c "curl http://localhost:8080/metrics"
```

On native execution, it is just a simple `curl` native request:

```bash
$> curl http://localhost:8080/metrics
```

Metrics implemented could be divided **counters**, **gauges** or **histograms**:

- **Counters**:
  - Processed requests (successful/errored)
  - Processed responses (successful/timed-out)
  - Non provisioned requests
  - Purged contexts (successful/failed)
  - File system and Unix sockets operations (successful/failed)


- **Gauges and histograms**:

  - Response delay seconds
  - Message size bytes for receptions
  - Message size bytes for transmissions



The metrics naming in this project, includes a family prefix which is the project applications name (`h2agent` or `udp_server_h2client`) and the endpoint category (`traffic_server`, `admin_server`, `traffic_client` for `h2agent`, and empty (as implicit), for `udp-server-h2client`). This convention and the labels provided (`[label] `: source, method, status_code, operation, result), are designed to ease metrics identification when using monitoring systems like [grafana](https://www.grafana.com).

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



#### HTTP2 clients

```
Counters provided by http2comm library:

   h2agent_traffic_client_observed_resquests_sents_counter [source] [method]
   h2agent_traffic_client_observed_resquests_unsent_counter [source] [method]
   h2agent_traffic_client_observed_responses_received_counter [source] [method] [status_code]
   h2agent_traffic_client_observed_responses_timedout_counter [source] [method]

Gauges provided by http2comm library:

   h2agent_traffic_client_responses_delay_seconds_gauge [source]
   h2agent_traffic_client_sent_messages_size_bytes_gauge [source]
   h2agent_traffic_client_received_messages_size_bytes_gauge [source]

Histograms provided by http2comm library:

   h2agent_traffic_client_responses_delay_seconds [source]
   h2agent_traffic_client_sent_messages_size_bytes [source]
   h2agent_traffic_client_received_messages_size_bytes [source]
```



As commented, same metrics described above, are also generated for the other application 'udp-server-h2client':



```
Counters provided by http2comm library:

   udp_server_h2client_observed_resquests_sents_counter [source] [method]
   udp_server_h2client_observed_resquests_unsent_counter [source] [method]
   udp_server_h2client_observed_responses_received_counter [source] [method] [status_code]
   udp_server_h2client_observed_responses_timedout_counter [source] [method]

Gauges provided by http2comm library:

   udp_server_h2client_responses_delay_seconds_gauge [source]
   udp_server_h2client_sent_messages_size_bytes_gauge [source]
   udp_server_h2client_received_messages_size_bytes_gauge [source]

Histograms provided by http2comm library:

   udp_server_h2client_responses_delay_seconds [source]
   udp_server_h2client_sent_messages_size_bytes [source]
   udp_server_h2client_received_messages_size_bytes [source]
```



Examples:

```bash
udp_server_h2client_responses_delay_seconds_bucket{source="udp_server_h2client",le="0.0001"} 21835
h2agent_traffic_client_observed_responses_timedout_counter{source="http2proxy_myClient"} 134
h2agent_traffic_client_observed_responses_received_counter{source="h2agent_myClient"} 9776
```

Note that 'histogram' is not part of histograms' category metrics name suffix (as counters and gauges do). The reason is to avoid confusion because metrics created are not actually histogram containers (except bucket). So, 'sum' and 'count' can be used to represent latencies, but not directly as histograms but doing some intermediate calculations:

```bash
rate(h2agent_traffic_client_responses_delay_seconds_sum[2m])/rate(h2agent_traffic_client_responses_delay_seconds_count[2m])
```

So, previous expression (rate is the mean variation in given time interval) is better without 'histogram' in the names, and helps to represent the latency updated in real time (every 2 minutes in the example).

#### HTTP2 servers

We have two groups of server metrics. One for administrative operations (1 administrative server interface) and one for traffic events (1 traffic server interface):

```
Counters provided by http2comm library and h2agent itself(*):

   h2agent_[traffic|admin]_server_observed_resquests_accepted_counter [source] [method]
   h2agent_[traffic|admin]_server_observed_resquests_errored_counter [source] [method]
   h2agent_[traffic|admin]_server_observed_responses_counter [source] [method] [status_code]
   h2agent_traffic_server_provisioned_requests_counter (*) [source] [result: successful/failed]
   h2agent_traffic_server_purged_contexts_counter (*) [source] [result: successful/failed]

Gauges provided by http2comm library:

   h2agent_[traffic|admin]_server_responses_delay_seconds_gauge [source]
   h2agent_[traffic|admin]_server_received_messages_size_bytes_gauge [source]
   h2agent_[traffic|admin]_server_sent_messages_size_bytes_gauge [source]

Histograms provided by http2comm library:

   h2agent_[traffic|admin]_server_responses_delay_seconds [source]
   h2agent_[traffic|admin]_server_received_messages_size_bytes [source]
   h2agent_[traffic|admin]_server_sent_messages_size_bytes [source]
```

For example:

```bash
h2agent_traffic_server_received_messages_size_bytes_bucket{source="myServer"} 38
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

#### UDP sockets

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
