# C++ HTTP/2 Mock Service

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://codedocs.xyz/testillano/h2agent.svg)](https://codedocs.xyz/testillano/h2agent/index.html)
[![Coverage Status](https://coveralls.io/repos/github/testillano/h2agent/badge.svg?branch=master&kill_cache=1)](https://coveralls.io/github/testillano/h2agent?branch=master)
[![Ask Me Anything !](https://img.shields.io/badge/Ask%20me-anything-1abc9c.svg)](https://github.com/testillano)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/testillano/h2agent/graphs/commit-activity)
[![CI](https://github.com/testillano/h2agent/actions/workflows/ci.yml/badge.svg)](https://github.com/testillano/h2agent/actions/workflows/ci.yml)

`H2agent` is a network service agent that enables **mocking other network services using HTTP/2 protocol**.
It is mainly designed for testing, but could even simulate or complement advanced services.

## Quick start

It is recommended to complete the following points first:

* A ***[prezi](https://prezi.com/view/RFaiKzv6K6GGoFq3tpui/)*** presentation to show a complete and useful overview of the `h2agent` component architecture.
* A ***[demo](./README.md#demo)*** exercise which presents a basic use case to better understand the project essentials.
* And finally, a ***[kata](./README.md#kata)*** training to adquire better knowledge of project capabilities.

## Scope

When developing a network service, one often needs to integrate it with other services. However, integrating full-blown versions of such services in a development setup is not always suitable, for instance when they are either heavyweight or not fully developed.

`H2agent` can be used to replace one (or many) of those, which allows development to progress and testing to be conducted in isolation against such a service.

`H2agent` supports HTTP2 as a network protocol and JSON as a data interchange language.

So, `h2agent` could be used as:

* **Server** mock: fully implemented
* **Client** mock: design ongoing (roadmap planned for 3.x.x).

Also, `h2agent` can be configured through **command-line** but also dynamically through an **administrative HTTP/2 interface** (`REST API`). This last feature makes the process a key element within an ecosystem of remotely controlled agents, enabling a reliable and powerful orchestration system to develop all kinds of functional, load and integration tests. So, in summary `h2agent` offers two execution planes:

* **Traffic plane**: application flows.
* **Control plane**: traffic flow orchestration, mocks behavior control and SUT surroundings monitoring and inspection.

Check the [releases](https://github.com/testillano/h2agent/releases) to get latest packages, or read the following sections to build all the artifacts needed to start playing:

## How can you use it ?

`H2agent` process may be used natively, as a `docker` container, or as part of `kubernetes` deployment.

The easiest way to build the project is using [containers](https://en.wikipedia.org/wiki/LXC) technology (this project uses `docker`): **to generate all the artifacts**, just type the following:

```bash
$> ./build.sh --auto
```

The option `--auto` builds the <u>builder image</u> (`--builder-image`) , then the <u>project image</u> (`--project-image`) and finally the <u>project executable</u> (`--project`). Then you will have everything available to run the process with three different modes:

* Run <u>project executable</u> natively (standalone):

  ```bash
  $> build/Release/bin/h2agent & # default server at 0.0.0.0 with traffic/admin/prometheus ports: 8000/8074/8080
  ```

  You may play with native helpers functions and examples:

  ```bash
  $> source tools/helpers.src # type help in any moment after sourcing
  $> server_example # follow instructions or just source it: source <(server_example)
  ```

  You could also provide `-h` or `--help` to get **process help**: more information [here](#execution-of-main-agent).

* Run <u>project image</u> with docker:

  ```bash
  $> docker run --network=host --rm -it ghcr.io/testillano/h2agent:latest & # you may play native helpers again, on host
  ```

* Run within `kubernetes` deployment: corresponding `helm charts` are normally packaged into releases. This is described in ["how it is delivered"](#how-it-is-delivered) section, but in summary, you could do the following:

  ```bash
  $> # helm dependency update helm/h2agent # no dependencies at the moment
  $> helm install h2agent-example helm/h2agent --wait
  $> pod=$(kubectl get pod -l app.kubernetes.io/name=h2agent --no-headers -o name)
  $> kubectl exec ${pod} -c h2agent -- /opt/h2agent -h
  ```

  You may enter the pod and play with helpers functions and examples which are also deployed with the chart under `/opt/utils` and automatically sourced on `bash` shell:

  ```bash
  $> kubectl exec -it ${pod} -- bash
  ```



Next sections will describe in detail, how to build [project image](#project-image) and project executable ([using docker](#build-project-with-docker) or [natively](#build-project-natively)).

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

### Usage

Builder image is used to build the executable. To run compilation over this image, again, just run with `docker`:

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
$> ./build-native.sh
```

Note: this script is tested on `ubuntu bionic`, then some requirements could be not fulfilled in other distributions.



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
$ cmake -DMY_OWN_INSTALL_PREFIX=$HOME/myPrograms/http2
$ make install
```

### Uninstall

```bash
$ cat install_manifest.txt | sudo xargs rm
```

## Testing

### Unit test

*Work ongoing*: check the badge above to know the current coverage level.
You can execute it after project building, for example for `Release` target: `./build/Release/bin/unit-test`.

#### Coverage

Unit test coverage could be easily calculated executing the script `./tools/coverage.sh`. This script builds and runs an image based in `./Dockerfile.coverage` which uses the `lcov` utility behind. Finally, a `firefox` instance is launched showing the coverage report where you could navigate the source tree to check the current status of the project. This stage is also executed as part of `h2agent` continuous integration (`github workflow`).

Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.

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

### Benchmarking test

This test is useful to identify possible memory leaks, process crashes or performance degradation introduced with new fixes or features.

Reference:

* VirtualBox VM with Linux Bionic (Ubuntu 18.04.3 LTS).

* Running on Intel(R) Core(TM) i7-8650U CPU @1.90GHz.

* Memory size: 15GiB.



Load testing is done with both [h2load](https://nghttp2.org/documentation/h2load-howto.html) and [hermes](https://github.com/jgomezselles/hermes) utilities using the helper script `st/start.sh` (check `-h|--help` for more information).

Also, `st/last.sh` script repeats the last execution in headless mode.

As schema validation is normally used only for function tests, it will be disabled here:

```bash
$ st/start.sh -y


Input Validate schemas (y|n)
 (or set 'H2AGENT_VALIDATE_SCHEMAS' to be non-interactive) [n]: n

Input Matching configuration
 (or set 'H2AGENT_SERVER_MATCHING' to be non-interactive) [server-matching.json]: server-matching.json

Input Provision configuration
 (or set 'H2AGENT_SERVER_PROVISION' to be non-interactive) [server-provision.json]: server-provision.json

Input Global variable(s) configuration
 (or set 'H2AGENT_GLOBAL_VARIABLE' to be non-interactive) [global-variable.json]: global-variable.json

Input Server data storage configuration (discard-all|discard-history|keep-all)
 (or set 'H2AGENT__DATA_STORAGE_CONFIGURATION' to be non-interactive) [discard-all]: discard-all

Input Server data purge configuration (enable-purge|disable-purge)
 (or set 'H2AGENT__DATA_PURGE_CONFIGURATION' to be non-interactive) [disable-purge]: disable-purge

Input H2agent endpoint address
 (or set 'H2AGENT__BIND_ADDRESS' to be non-interactive) [0.0.0.0]: 0.0.0.0

Input H2agent response delay in milliseconds
 (or set 'H2AGENT__RESPONSE_DELAY_MS' to be non-interactive) [0]: 0

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
}

# Invoke the function:
random_request


Input Request url
 (or set 'ST_REQUEST_URL' to be non-interactive) [/app/v1/load-test/v1/id-21]:

Server data configuration:
{"purgeExecution":"false","storeEvents":"false","storeEventsKeyHistory":"false"}

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

finished in 1.57s, 63542.98 req/s, 66.30MB/s
requests: 100000 total, 100000 started, 100000 done, 100000 succeeded, 0 failed, 0 errored, 0 timeout
status codes: 100000 2xx, 0 3xx, 0 4xx, 0 5xx
traffic: 104.34MB (109407063) total, 293.03KB (300058) headers (space savings 95.77%), 102.33MB (107300000) data
                     min         max         mean         sd        +/- sd
time for request:      658us      2.88ms      1.56ms       176us    83.99%
time for connect:      148us       148us       148us         0us   100.00%
time to 1st byte:     1.10ms      1.10ms      1.10ms         0us   100.00%
req/s           :   63552.33    63552.33    63552.33        0.00   100.00%

real    0m1.579s
user    0m0.126s
sys     0m0.107s
+ set +x

Created test report:
  last -> ./report_delay0_iters100000_c1_t1_m100.txt
```

## Execution of main agent

### Command line

You may take a look to `h2agent` command line by just typing the build path, for example for `Release` target: `./build/Release/bin/h2agent -h|--help`:

```
./build/Release/bin/h2agent -h
h2agent - HTTP/2 Agent service

Usage: h2agent [options]

Options:

[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]
  Set the logging level; defaults to warning.

[--verbose]
  Output log traces on console.

[--ipv6]
  IP stack configured for IPv6. Defaults to IPv4.

[-b|--bind-address <address>]
  Servers bind <address> (admin/traffic/prometheus); defaults to '0.0.0.0' (ipv4) or '::' (ipv6).

[-a|--admin-port <port>]
  Admin <port>; defaults to 8074.

[-p|--traffic-server-port <port>]
  Traffic server <port>; defaults to 8000. Set '-1' to disable
  (mock server service is enabled by default).

[-m|--traffic-server-api-name <name>]
  Traffic server API name; defaults to empty.

[-n|--traffic-server-api-version <version>]
  Traffic server API version; defaults to empty.

[-w|--traffic-server-worker-threads <threads>]
  Number of traffic server worker threads; defaults to 1, which should be enough
  even for complex logic provisioned (admin server always uses 1 worker thread).
  It could be increased if hardware concurrency permits a greater margin taking
  into account other process threads considered busy.

[-t|--traffic-server-threads <threads>]
  Number of nghttp2 traffic server threads; defaults to 2 (2 connections)
  (admin server hardcodes 2 nghttp2 threads). This option is exploited
  by multiple clients.

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

[--discard-data]
  Disables data storage for events processed (enabled by default).
  This invalidates some features like FSM related ones (in-state, out-state)
  or event-source transformations.

[--discard-data-key-history]
  Disables data key history storage (enabled by default).
  Only latest event (for each key 'method/uri') will be stored and will
  be accessible for further analysis.
  This limits some features like FSM related ones (in-state, out-state)
  or event-source transformations.
  Implicitly disabled by option '--discard-data'.
  Ignored for server-unprovisioned events (for troubleshooting purposes).

[--disable-purge]
  Skips events post-removal when a provision on 'purge' state is reached (enabled by default).

[--prometheus-port <port>]
  Prometheus <port>; defaults to 8080.

[--prometheus-response-delay-seconds-histogram-boundaries <space-separated list of doubles>]
  Bucket boundaries for response delay seconds histogram; no boundaries are defined by default.
  Scientific notation is allowed, so in terms of microseconds (e-6) and milliseconds (e-3) we
  could provide, for example: "100e-6 200e-6 300e-6 400e-6 500e-6 1e-3 5e-3 10e-3 20e-3".

[--prometheus-message-size-bytes-histogram-boundaries <space-separated list of doubles>]
  Bucket boundaries for Rx/Tx message size bytes histogram; no boundaries are defined by default.

[--disable-metrics]
  Disables prometheus scrape port (enabled by default).

[-v|--version]
  Program version.

[-h|--help]
  This help.
```

## Execution of matching helper utility

This utility could be useful to test regular expressions before putting them at provision objects (`requestUri` or transformation filters which use regular expressions).

### Command line

You may take a look to `matching-helper` command line by just typing the build path, for example for `Release` target:

```bash
$> ./build/Release/bin/matching-helper -h
Usage: matching-helper [options]

Options:

--regex <value>
  Regex pattern value to match against.

--test <value>
  Test string value to be matched.

[--fmt <value>]
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
$> ./build/Release/bin/matching-helper --regex "(a\|b\|)([0-9]{10})" --test "a|b|0123456789" --fmt '$2'

Regex: (a\|b\|)([0-9]{10})
Test:  a|b|0123456789
Fmt:   $2

Match result: true
Fmt result  : 0123456789
```

## Execution of Arash Partow's helper utility

This utility could be useful to test [Arash Partow's](https://github.com/ArashPartow/exprtk) mathematical expressions before putting them at provision objects (`math.*` source).

### Command line

You may take a look to `arashpartow-helper` command line by just typing the build path, for example for `Release` target:

```bash
$> ./build/Release/bin/arashpartow-helper -h
Usage: arashpartow-helper [options]

Options:

--expression <value>
  Expression to be calculated.

[-h|--help]
  This help.

Examples:
   arashpartow-helper --expression "(1+sqrt(5))/2"
   arashpartow-helper --expression "404 == 404"
   arashpartow-helper --expression "cos(3.141592)"
```

Execution example:

```bash
$> ./build/Release/bin/arashpartow-helper --expression "404 == 404"

Expression: 404 == 404

Result: 1
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

## Metrics

Based in [prometheus data model](https://prometheus.io/docs/concepts/data_model/) and implemented with [prometheus-cpp library](https://github.com/jupp0r/prometheus-cpp), those metrics are collected and exposed through the server scraping port (`8080` by default, but configurable at [command line](#command-line) by mean `--prometheus-port` option) and could be retrieved using Prometheus or compatible visualization software like [Grafana](https://prometheus.io/docs/visualization/grafana/) or just browsing `http://localhost:8080/metrics`.

More information about implemented counters [here](#OAM).

## Traces and printouts

Traces are managed by `syslog` by default, but could be shown verbosely at standard output (`--verbose`) depending on the traces design level and the current level assigned. For example:

```bash
$ ./h2agent --verbose &
[1] 27407
[03/04/21 20:49:35 CEST] Starting h2agent v1.0.0-93-g0ab2129
Log level: Warning
Verbose (stdout): true
IP stack: IPv4
Admin port: 8074
Traffic server (mock server service): enabled
Traffic server bind address: 0.0.0.0
Traffic server port: 8000
Traffic server api name: <none>
Traffic server api version: <none>
Traffic server threads (nghttp2): 2
Traffic server worker threads: 1
Traffic server key password: <not provided>
Traffic server key file: <not provided>
Traffic server crt file: <not provided>
SSL/TLS disabled: both key & certificate must be provided
Traffic secured: no
Admin secured: no
Schema configuration file: <not provided>
Global variables configuration file: <not provided>
Data storage: enabled
Data key history storage: enabled
Purge execution: enabled
Traffic server matching configuration file: <not provided>
Traffic server provision configuration file: <not provided>
Prometheus bind address: 0.0.0.0
Prometheus port: 8080

$ kill $!
[Warning]|/code/src/main.cpp:114(sighndl)|Signal received: 15
[Warning]|/code/src/main.cpp:104(_exit)|Terminating with exit code 1
[Warning]|/code/src/main.cpp:134(stopAgent)|Stopping h2agent timers service at 13/05/22 20:07:59 CEST
[Warning]|/code/src/main.cpp:142(stopAgent)|Stopping h2agent admin service at 13/05/22 20:07:59 CEST
[Warning]|/code/src/main.cpp:149(stopAgent)|Stopping h2agent traffic service at 13/05/22 20:07:59 CEST
[Warning]|/code/src/main.cpp:160(_exit)|Stopping logger

[1]+  Exit 1                  h2agent --verbose
```

## Training

### Demo

A demo is available at `./demo` directory. It is designed to introduce the `h2agent` in a funny way with an easy use case. Open its [README.md](./demo/README.md) file to learn more about.
Just in case you want to test demo procedure health, you can execute in non-interactive mode:

```bash
$> INTERACT=false demo/run.sh
$> echo $?
```

### Kata

A kata is available at `./kata` directory. It is designed to guide through a set of exercises with increasing complexity. Check its [README.md](./kata/README.md) file to learn more about.

### Working with docker

Sometimes, `github` access restrictions to build the project from scratch could be a handicap. Other times, you could simple prefer to run training stuff isolated.

So you could find useful to run the corresponding docker container using the script `./tools/training.sh`. This script builds and runs an image based in `./Dockerfile.training` which adds the needed resources to run both `demo` and `kata`. The image working directory is `/home/h2agent` making the experience like working natively over the git checkout.

The training image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull ghcr.io/testillano/h2agent_training:<tag>
```
Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.

## Management interface

`h2agent` listens on a specific management port (*8074* by default) for incoming requests, implementing a *REST API* to manage the process operation. Through the *API* we could program the agent behavior. The following sections describe all the supported operations over *URI* path`/admin/v1/`.

We will start describing **general** mock operations:

* Schemas: define validation schemas used in further provisions to check the incoming and outgoing traffic.
* Global variables: shared variables between different provision contexts and flows. Normally not needed, but it is an extra feature to solve some situations by other means.
* Logging: dynamic logger configuration (update and check).

Then, we will describe **traffic server mock** features:

* Server matching configuration: classification algorithms to  split the incoming traffic and access to the final procedure which will be applied.

* Server provision configuration: here we will define the mock behavior regarding the request received, and the transformations done over it to build the final response and evolve, if proceed, to another state for further receptions.

* Server data storage: data inspection is useful for both external queries (mainly troubleshooting) and internal ones (provision transformations).



___



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

Loads global variables for future usage.

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

### GET /admin/v1/global-variable

Global variables are created dynamically during provision processing and can be used in that provision or in any other one. This operation retrieves the whole list of global variables created:

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

Json object document with variable fields and their values, when something is stored (no-content response has no body).

Take the following `json` as an example:

```json
{
  "variable_name_1": "variable_value_1",
  "variable_name_2": "variable_value_2",
  "variable_name_3": "variable_value_3"
}
```

### DELETE /admin/v1/global-variable

Deletes all the global variables registered.

#### Response status code

**200** (OK) or **204** (No Content).

#### Response body

No response body.

### GET /admin/v1/logging

Retrieves the current logging level of the `h2agent` process: `Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency`.

#### Response status code

**200** (OK).

#### Response body

String containing the current log level name.

### PUT /admin/v1/logging?level=`<level>`

Changes the log level of the `h2agent` process to any of the available levels (this can be also configured on start as described in [command line](#command-line) section). So, `level` query parameter value could be any of the valid log levels: `Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency`.

#### Response status code

**200** (OK) or **400** (Bad Request).



___



### POST /admin/v1/server-matching

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
        "enum": ["FullMatching", "FullMatchingRegexReplace", "PriorityMatchingRegex", "RegexMatching"]
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

So, this `regex-replace` algorithm is flexible enough to cover many possibilities (even *tokenize* path query parameters). As future proof, other fields could be added, like algorithm flags defined in underlying C++ `regex` standard library used.

Also, `regex-replace` could act as a virtual *full matching* algorithm when the transformation fails (the result will be the original tested key), because it can be used as a <u>fall back to cover non-strictly matched receptions</u>. The limitation here is when those unmatched receptions have variable parts (it is impossible/unpractical to provision all the possibilities). So, this fall back has sense to provision constant reception keys (fixed and predictable *URIs*), and of course, strict provision keys matching the result of `regex-replace` transformation on their reception keys which does not fit the other fall back ones.

###### RegexMatching

Deprecates `PriorityMatchingRegex`.

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
        {"required": ["AppendVar"]},
        {"required": ["PrependVar"]},
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
        "AppendVar": { "type": "string" },
        "PrependVar": { "type": "string" },
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
      "oneOf": [
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
            "pattern": "^request\\.(uri(\\.(path$|param\\..+))?|body(\\..+)?|header\\..+)$|^response\\.body(\\..+)?$|^eraser$|^math\\..*|^random\\.[-+]{0,1}[0-9]+\\.[-+]{0,1}[0-9]+$|^randomset\\..+|^timestamp\\.[m|n]{0,1}s$|^strftime\\..+|^recvseq$|^(var|globalVar|event)\\..+|^(value)\\..*|^inState$"
          },
          "target": {
            "type": "string",
            "pattern": "^response\\.body\\.(object$|object\\..+|jsonstring$|jsonstring\\..+|string$|string\\..+|integer$|integer\\..+|unsigned$|unsigned\\..+|float$|float\\..+|boolean$|boolean\\..+)|^response\\.(header\\..+|statusCode|delayMs)$|^(var|globalVar)\\..+|^outState(\\.(POST|GET|PUT|DELETE|HEAD)(\\..+)?)?$"
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

We could label a provision specification to take advantage of internal *FSM* (finite state machine) for matched occurrences. When a reception matches a provision specification, the real context is searched internally to get the current state ("**initial**" if missing or empty string provided) and then get the  `inState` provision for that value. Then, the specific provision is processed and the new state will get the `outState` provided value. This makes possible to program complex flows which depends on some conditions, not only related to matching keys, but also consequence from [transformation filters](#transform) which could manipulate those states.

These arguments are configured by default with the label "**initial**", used by the system when a reception does not match any internal occurrence (as the internal state is unassigned). This conforms a default rotation for further occurrences because the `outState` is again the next `inState`value. It is important to understand that if there is not at least 1 provision with `inState` = "**initial**" the matched occurrences won't never be processed. Also, if the next state configured (`outState` provisioned or transformed) has not a corresponding `inState` value, the flow will be broken/stopped.

So, "**initial**" is a reserved value which is mandatory to debut any kind of provisioned transaction. Remember that an empty string will be also converted to this special state for both `inState` and `outState` fields.

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

<u>Special **purge** state</u>:  stateful scenarios normally require access to former events (available at server data storage) to evolve through different provisions, so disabling server data is not an option to make them work properly. The thing is that high load testing could impact on memory consumption of the mock server if we don't have a way to clean information which is no longer needed and could be dangerously accumulated. Here is where purge operation gets importance: the keyword '*purge*' is a reserved out-state used to indicate that server data related to an event history must be dropped (it should be configured at the last scenario stage provision). This mechanism is useful in long-term load tests to avoid the commented high memory consumption removing those scenarios which have been successfully completed. A nice side-effect of this design, is that all the failed scenarios will be available for further analysis, as purge operation is performed at last scenario stage and won't be reached normally in this case of fail.

##### requestMethod

Expected request method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*).

##### requestUri

Request *URI* path (percent-encoded) to match depending on the algorithm selected. It includes possible query parameters, depending on matching filters provided for them.

<u>*Empty string is accepted*</u>, and is reserved to configure an optional default provision, something which could be specially useful to define the fall back provision if no matching entry is found. So, you could configure defaults for each method, just putting an empty *request URI* or omitting this optional field. Default provisions could evolve through states (in/out) but at least "initial" is again mandatory to be processed.

##### requestSchemaId

We could optionally validate requests against a `json` schema. Schemas are identified by string name and configured through [command line](#command-line) or [REST API](#management-interface). When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

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

Sorted list of transformations to modify incoming information and build the dynamic response to be sent.

Each transformation has a `source`, a `target` and an optional `filter` algorithm. <u>The filters are applied over sources and sent to targets</u> (all the available filters at the moment act over sources in string format, so they need to be converted if they are not strings in origin).

A best effort is done to transform and convert information to final target vaults, and when something wrong happens, a logging error is thrown and the transformation filter is skipped to move to the next one to be processed. For example, a source detected as *json* object cannot be assigned to a number or string target, but could be set into another *json* object.

Let's start describing the available sources of data: regardless the native or normal representation for every kind of target, the fact is that conversions may be done to almost every other type:

- *string* to *number* and *boolean* (true if non empty).

- *number* to *string* and *boolean* (true if different than zero).

- *boolean*: there is no source for boolean type, but you could create a non-empty string or non-zeroed number to represent *true* on a boolean target (only response body nodes could include a boolean).

- *json object*: request body node (whole document is indeed the root node) when being an object itself (note that it may be also a number or string). This data type can only be transfered into targets which support json objects like response body node.



*Variables substitution:*

Before describing sources and targets (and filters), just to clarify that in some situations it is allowed the insertion of variables in the form `@{var id}` which will be replaced if exist (**only normal variables can be used in that form**: global ones are excluded to avoid possible performance impact on load testing, but you could transfer a global variable into normal one to overcome this limitation). In that case we will add the comment "**admits variables substitution**". At certain sources and targets, substitutions are not allowed because have no sense or they are rarely needed:



The **source** of information is classified after parsing the following possible expressions:

- request.uri: whole `url-decoded` request *URI* (path together with possible query parameters).

- request.uri.path: `url-decoded` request *URI* path part.

- request.uri.param.`<name>`: request URI specific parameter `<name>`.

- request.body: request body document from *root*.

- request.body.`/<node1>/../<nodeN>`: request body node path. This source path **admits variables substitution**. Leading slash is needed as first node is considered the `json` root.

- response.body: response body document from *root*. The use of provisioned response as template reference is rare but could ease the build of `json` structures for further transformations.

- response.body.`/<node1>/../<nodeN>`: response body node path. This source path **admits variables substitution**. The use of provisioned response as template reference is rare but could ease the build of `json` structures for further transformations.

- request.header.`<hname>`: request header component (i.e. *content-type*). Take into account that header fields values are received [lower cased](https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2).

- eraser: this is used to indicate that the *target* specified (next section) must be removed or reset. Some of those targets are:
  - response node: there is a twisted use of the response body as a temporary test-bed template. It consists in inserting auxiliary nodes to be used as valid sources within provision transformations, and remove them before sending the response. Note that nonexistent nodes become null nodes when removed, so take care if you don't want this.
  - global variable: the user should remove this kind of variables after last flow usage to avoid memory grow in load testing. Global variables are not confined to an specific provision context (where purge procedure is restricted to the event history server data), so the eraser is the way to proceed when it comes to free the global list and reduce memory consumption.
  - With other kind of targets, eraser acts like setting an empty string.

- math.`<expression>`: this source is based in [Arash Partow's exprtk](https://github.com/ArashPartow/exprtk) math library compilation. There are many possibilities (calculus, control and logical expressions, trigonometry, logic, string processing, etc.), so check [here](https://github.com/ArashPartow/exprtk/blob/master/readme.txt) for more information. This source specification **admits variables substitution** (third-party library variable substitutions are not needed, so they are not supported). Some simple examples could be: "2*sqrt(2)", "sin(3.141592/2)", "max(16,25)", "1 and 1", etc. You may implement a simple arithmetic server (check [this](./kata/09.Arithmetic_Server/README.md) kata exercise to deepen the topic).

- random.`<min>.<max>`: integer number in range `[min, max]`. Negatives allowed, i.e.: `"-3.+4"`.

- randomset.`<value1>|..|<valueN>`: random string value between pipe-separated labels provided. This source specification **admits variables substitution**.

- timestamp.`<unit>`: UNIX epoch time in `s` (seconds), `ms` (milliseconds) or `ns` (nanoseconds).

- strftime.`<format>`: current date/time formatted by [strftime](https://www.cplusplus.com/reference/ctime/strftime/). This source format **admits variables substitution**.

- recvseq: sequence id number increased for every mock reception (starts on *1* when the *h2agent* is started).

- var.`<id>`: general purpose variable (accessible within transformation chain). Cannot refer json objects. This source variable identifier **admits variables substitution**.

- globalVar.`<id>`: general purpose global variable (readable from any provision). Cannot refer json objects. This source variable identifier **admits variables substitution**. Global variables are useful to store dynamic information to be used in a different provision instance. For example you could split a request `URI` in the form `/update/<id>/<timestamp>` and store a variable with the name `<id>` and value `<timestamp>`. That variable could be queried later just providing `<id>` which is probably enough in such context. Thus, we could parse other provisions (access to events addressed with dynamic elements), simulate advanced behaviors, or just parse mock invariant globals over configured provisions (although this seems to be less efficient than hard-coding them, it is true that it drives provisions adaptation "on the fly" if you update such globals when needed).

- value.`<value>`: free string value. Even convertible types are allowed, for example: integer string, unsigned integer string, float number string, boolean string (true if non-empty string), will be converted to the target type. Empty value is allowed, for example, to set an empty string, just type: `"value."`. This source value **admits variables substitution**.

- event.`<var id prefix>`: access server context indexed by request *method*, *URI* and requests *number* given by event variable prefix identifier in such a way that three general purpose variables must be available, as well as a fourth one  which will be the `json` path within the resulting selection. The names to store all the information are composed by the variable prefix name and the following four suffixes:

  - `<var id prefix>`.method: any supported method (*POST*, *GET*, *PUT*, *DELETE*, *HEAD*).
  - `<var id prefix>`.uri: event *URI* selected.
  - `<var id prefix>`.number: position selected (*1..N*; *-1 for last*) within events requests list.
  - `<var id prefix>`.path: `json` document path within selection.

  All the variables should be configured to load the source with the expected `json` object resulting from the selection (normally, it should be a single requests event and then access the `requestBody` field which transports the original **body** which was received by the selected event, but anyway, knowing the server data definition, any part could be accessed depending on the selection result, but take into account that [json pointers](https://tools.ietf.org/html/rfc6901) (as *path* is a `json pointer` definition) do not support accessing array elements, so it is recommended to specify a full selection including the *number*. Indeed, you already will know the *method* and *uri* so you don't need to extract them from the selection again).

  <u>Server requests history</u> should be kept enabled allowing to access not only the last event for a given selection key, but some scenarios could live without it.

  Let's see an example. Imagine the following current server data map:

  ```json
  [
    {
      "method": "POST",
      "requests": [
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
      "uri": "/app/v1/stock/madrid?loc=123"
    }
  ]
  ```

  Then, you could define an event source like `event.ev1`. Assuming that the following variables are available when this source is processed:

  ​	`ev1.method` = "POST"

  ​	`ev1.uri` = "/app/v1/stock/madrid?loc=123"

  ​	`ev1.number` = -1 (means "the last")

  ​	`ev1.path` = "/requestBody"

  Then, the event source (`event.ev1`) would store this `json` object:

  ```json
  {
    "engine": "tdi",
    "model": "audi",
    "year": 2021
  }
  ```

  In the same way you could address internal event nested objects and also leaf nodes with basic types (`ev1.path` = "/requestBody/engine" would retrieve "tdi" string as the event data source).

- inState: current processing state.



The **target** of information is classified after parsing the following possible expressions (between *[square brackets]* we denote the potential data types allowed):

- response.body.string *[string]*: response body document storing expected string at *root*.

- response.body.integer *[integer]*: response body document storing expected integer at *root*.

- response.body.unsigned *[unsigned integer]*: response body document storing expected unsigned integer at *root*.

- response.body.float *[float number]*: response body document storing expected float number at *root*.

- response.body.boolean *[boolean]*: response body document storing expected boolean at *root*.

- response.body.object *[json object]*: response body document storing expected object as *root* node.

- response.body.jsonstring *[json string]*: response body document storing expected object, extracted from json-parsed string, as *root* node.

- response.body.string.`/<node1>/../<nodeN>` *[string]*: response body node path storing expected string. This target path **admits variables substitution**.

- response.body.integer.`/<node1>/../<nodeN>` *[integer]*: response body node path storing expected integer. This target path **admits variables substitution**.

- response.body.unsigned.`/<node1>/../<nodeN>` *[unsigned integer]*: response body node path storing expected unsigned integer. This target path **admits variables substitution**.

- response.body.float.`/<node1>/../<nodeN>` *[float number]*: response body node path storing expected float number. This target path **admits variables substitution**.

- response.body.boolean.`/<node1>/../<nodeN>` *[boolean]*: response body node path storing expected booblean. This target path **admits variables substitution**.

- response.body.object.`/<node1>/../<nodeN>` *[json object]*: response body node path storing expected object under provided path. If source origin is not an object, there will be a best effort to convert to string, number, unsigned number, float number and boolean, in this specific priority order. This target path **admits variables substitution**.

- response.body.jsonstring.`/<node1>/../<nodeN>` *[json string]*: response body node path storing expected object, extracted from json-parsed string, under provided path. This target path **admits variables substitution**.

- response.header.`<hname>` *[string (or number as string)]*: response header component (i.e. *location*). This target name **admits variables substitution**. Take into account that header fields values are sent [lower cased](https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2).

- response.statusCode *[unsigned integer]*: response status code.

- response.delayMs *[unsigned integer]*: simulated delay to respond: although you can configure a fixed value for this property on provision document, this transformation target overrides it.

- var.`<id>` *[string (or number as string)]*: general purpose variable (intended to be used as source later). The idea of *variable* vaults is to optimize transformations when multiple transfers are going to be done (for example, complex operations like regular expression filters, are dumped to a variable, and then, we drop its value over many targets without having to repeat those complex algorithms again). Cannot store json objects. This target variable identifier **admits variables substitution**.

- globalVar.`<id>` *[string (or number as string)]*: general purpose global variable (intended to be used as source later; writable from any provision). Cannot refer json objects. This target variable identifier **admits variables substitution**.

- outState *[string (or number as string)]*: next processing state. This overrides the default provisioned one.

- outState.`[POST|GET|PUT|DELETE|HEAD][.<uri>]` *[string (or number as string)]*: next processing state for specific method (virtual server data will be created if needed: this way we could modify the flow for other methods different than the one which is managing the current provision). This target **admits variables substitution** in the `uri` part.

  You could, for example, simulate a database where a *DELETE* for an specific entry could infer through its provision an *out-state* for a foreign method like *GET*, so when getting that *URI* you could obtain a *404* (assumed this provision for the new *working-state* = *in-state* = *out-state* = "id-deleted"). By default, the same `uri` is used from the current event to the foreign method, but it could also be provided optionally giving more flexibility to generate virtual events with specific states.




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
    "target": "response.body.string.category",
    "filter": { "RegexCapture" : "\/api\/v2\/id-[0-9]+\/category-([a-z]+)" }
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




- AppendVar: this appends a variable value to the source:

  ```json
  {
    "source": "value.I am engineer and my name is ",
    "target": "var.biography",
    "filter": { "AppendVar" : "name" }
  }
  ```

  In the example above we append the value of variable *name* to a constant-value source, so will have *var.biography="I am engineer and my name is  <value of variable 'name'>"*.



- PrependVar: this prepends a variable value to the source:

  ```json
  {
    "source": "value.. I'm currently working with C++",
    "target": "var.biography2",
    "filter": { "PrependVar" : "biography" }
  }
  ```

  Taking as reference the previous example variable *biography*, we will prepend it to a new constant-value source, so will have *var.biography2="I am engineer and my name is  <value of variable 'name'>. I'm currently working with C++"*.

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



- Multiply: multiplies the source (if numeric conversion is possible) by the value provided (which <u>also could be negative to change sign, or lesser than 1 to divide</u>):

  ```json
  {
    "source": "value.-10",
    "target": "var.value-of-one",
    "filter": { "Multiply" : -0.1 }
  }
  ```

  In this example, we operate `-10 * -0.1 = 1`. For more complex operations, you may use the `math` source.



- ConditionVar: conditional transfer from source to target based in boolean interpretation of the provided variable value. All the variables are strings in origin, and are converted to target selected types, but in this case the string variable value is adapted to boolean result in this way: if the variable is not defined or it is empty, the condition is *false*. It will be *true*  in the rest of cases.

  Note that a variable containing the literal "false" would be interpreted as *true*, and also a `math` transformation for `404==503` which becomes `0.00000` is also interpreted as *true* because it is a non-empty string.

  This behavior invite to use regular expressions matches as booleans (a target variable stores the source match when using *RegexCapture* filter). for example:

  ```json
  {
    "source": "request.body./forceErrors/internalServerError",
    "target": "var.internalServerError"
  },
  {
    "source": "value.500",
    "target": "response.statusCode",
    "filter": { "ConditionVar" : "internalServerError" }
  }
  ```

  In this example, the request body dictates the responses' status code depending on value (empty or not) received at "*/forceErrors/internalServerError*". Of course there are many ways to set the condition variable depending on the needs.




Finally, after possible transformations, we could validate the response body:

##### responseSchemaId

We could optionally validate built responses against a `json` schema. Schemas are identified by string name and configured through [command line](#command-line) or [REST API](#management-interface). When a referenced schema identifier is not yet registered, the provision processing will ignore it with a warning. This allows to enable schemas validation on the fly after traffic flow initiation, or disable them before termination.

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

Response status codes and body content follow same criteria than single provisions. A provision set fails with the first failed item, giving a 'pluralized' version of the single provision failed response message.

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
*  `discard=false&discardKeyHistory=true`: no key history stored (only the last event for a key, except for unprovisioned events, which history is always respected for troubleshooting purposes).
* `discard=false&discardKeyHistory=false`: everything is stored: events and key history.

The combination `discard=true&discardKeyHistory=false` is incoherent, as it is not possible to store requests history with general events discarded. In this case, an status code *400 (Bad Request)* is returned.

And regardless the previous combinations, you could enable or disable the purge execution when this reserved state is reached for a specific provision. Take into account that this stage has no sense if no data is stored but you could configure it anyway:

* `disablePurge=true`: provisions with `purge` state will ignore post-removal operation when this state is reached.
* `disablePurge=false`: provisions with `purge` state will process post-removal operation when this state is reached.

The `h2agent` starts with purge stage enabled by default, but you could also change this through command-line (`--disable-purge`).

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
    "purgeExecution": "true",
    "storeEvents": "true",
    "storeEventsKeyHistory": "true"
}
```

By default, the `h2agent` enables both kinds of storage types (general events and requests history events), and also enables the purge execution if any provision with this state is reached, so the previous response body will be returned on this query operation. This is useful for function/component testing where more information available is good to fulfill the validation requirements. In load testing, we could seize the `purge` out-state to control the memory consumption, or even disable storage flags in case that test plan is stateless and allows to do that simplification.

### GET /admin/v1/server-data?requestMethod=`<method>`&requestUri=`<uri>`&requestNumber=`<number>`

Retrieves the current server internal data (requests received, their states and other useful information like timing or global order). Events received are stored <u>even if no provisions were found</u> for them (the agent responds with `501`, not implemented), being useful to troubleshoot possible configuration mistakes in the tests design. By default, the `h2agent` stores the whole history of events (for example requests received for the same `method` and `uri`) to allow advanced manipulation of further responses based on that information.

<u>Without query parameters</u> (`GET /admin/v1/server-data`), you may be careful with large contexts born from long-term tests (load testing), because a huge response could collapse the receiver (terminal or piped process). With query parameters, you could filter a specific entry providing *requestMethod*, *requestUri* and <u>optionally</u> a *requestNumber*, for example:

`/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&requestNumber=3`

The `json` document response shall contain three main nodes: `method`, `uri` and a `requests` object with the chronologically ordered list of events received for the given `method/uri` combination.

Both *method* and *uri* shall be provided together (if any of them is missing, a bad request is obtained), and *requestNumber* cannot be provided alone as it is an additional filter which selects the history item for the `method/uri` key (the response `requests` node will contain a single register in this case). So, the *requestNumber* is the history position, **1..N** in chronological order, and **-1..-N** in reverse chronological order (latest one by mean -1 and so on). The zeroed value is not accepted.

This operation is useful for testing post verification stages (validate content and/or document schema for an specific interface). Remember that you could start the *h2agent* providing a requests schema file to validate incoming receptions through traffic interface, but external validation allows to apply different schemas (although this need depends on the application that you are mocking), and also permits to match the requests content that the agent received.

#### Response status code

**200** (OK), **204** (No Content) or **400** (Bad Request).

#### Response body

Json array document containing all the selected event items, when something matches (no-content response has no body).

When provided *method* and *uri*, server data will be filtered with that key. If request number is provided too, the single event object, if exists, will be returned. When no query parameters are provided, the whole internal data organized by key (*method* + *uri* ) together with their requests arrays are returned.

Example of whole structure for a unique key (*GET* on '*/app/v1/foo/bar/1?name=test*'):

```json
[
  {
    "method": "GET",
    "requests": [
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



Example of single event for a unique key (*GET* on '*/app/v1/foo/bar/1?name=test*') and a *requestNumber* (2):

```json
[
  {
    "method": "GET",
    "requests": [
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



The information collected for a requests item is:

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
* `serverSequence`: current server monotonically increased sequence for every reception. In case of a virtual register (if it contains the field `virtualOrigin`), this sequence is actually not increased for the server data entry shown, only for the original event which caused this one.

### GET /admin/v1/server-data/summary?maxKeys=`<number>`

When a huge amount of events are stored, we can still troubleshoot an specific known key by mean filtering the server data as commented in the previous section. But if we need just to check what's going on there (imagine a high amount of failed transactions, thus not purged), perhaps some hints like the total amount of receptions or some example keys may be useful to avoid performance impact in the server due to the unfiltered query, as well as difficult forensics of the big document obtained. So, the purpose of server data summary operation is try to guide the user to narrow and prepare an efficient query.

#### Response status code

**200** (OK).

#### Response body

A `json` object document with some practical information is built:

* `displayedKeys`: the summary could also be too big to be displayed, so query parameter *maxKeys* will limit the number (`amount`) of displayed keys in the whole response. Each key in the `list` is given by the *method* and *uri*, and also the number of history requests (`amount`) is shown.
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

### DELETE /admin/v1/server-data

Deletes the server data given by query parameters defined in the same way as former *GET* operation. For example:

`/admin/v1/server-data?requestMethod=GET&requestUri=/app/v1/foo/bar/5&requestNumber=3`

Same restrictions apply here for deletion: query parameters could be omitted to remove everything, *method* and *URI* are provided together and *requestNumber* restricts optionally them.

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
       version: 2.2.0
       repository: alias:erthelm
       alias: h2server

     - name: h2agent
       version: 2.2.0
       repository: alias:erthelm
       alias: h2server2
   ```

3. Refer to `h2agent` values through the corresponding dependency alias, for example `.Values.h2server.image` to access process repository and tag.

### Agent configuration files

Some [command line](#command-line) arguments used by the `h2agent` process are files, so they could be added by mean a `config map` (key & certificate for secured connections and matching/provision configuration files).

## Troubleshooting

### Helper functions

As we commented [above](#how-it-is-delivered), the `h2agent` helm chart packages a helper functions script which is very useful for troubleshooting. This script is also available for native usage (`./tools/helpers.src`):

```bash
source tools/helpers.src

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
Usage: schema_schema [-h|--help]; Gets the schema configuration schema
                                  (http://localhost:8074/admin/v1/schema/schema).
Usage: global_variable [-h|--help] [--clean] [file]; Cleans/gets/updates current agent global variable configuration
                                                     (http://localhost:8074/admin/v1/global-variable).
Usage: global_variable_schema [-h|--help]; Gets the agent global variable configuration schema
                                           (http://localhost:8074/admin/v1/global-variable/schema).
Usage: data_configuration [-h|--help]; Manages agent data configuration (gets current status by default).
                          [--discard-all]     ; Sets data configuration to discard all the events processed.
                          [--discard-history] ; Sets data configuration to keep only the last event processed for a key.
                          [--keep-all]        ; Sets data configuration to keep all the events processed.
                          [--disable-purge]   ; Sets data configuration to skip events post-removal when a provision on
                                                'purge' state is reached.
                          [--enable-purge]    ; Sets data configuration to process events post-removal when a provision on
                                                'purge' state is reached.
=== Traffic server ===
Usage: server_matching [-h|--help]; Gets/updates current server matching configuration
                                    (http://localhost:8074/admin/v1/server-matching).
Usage: server_matching_schema [-h|--help]; Gets the server matching configuration schema
                                           (http://localhost:8074/admin/v1/server-matching/schema).
Usage: server_provision [-h|--help] [--clean] [file]; Cleans/gets/updates current server provision configuration
                                                      (http://localhost:8074/admin/v1/server-provision).
Usage: server_provision_schema [-h|--help]; Gets the server provision configuration schema
                                            (http://localhost:8074/admin/v1/server-provision/schema).
Usage: server_data [-h|--help]; Inspects server data events (http://localhost:8074/admin/v1/server-data).
                   [method] [uri] [[-]request number]; Restricts shown data with given positional filters.
                                                       Request number may be negative to access by reverse chronological order.
                   [--summary] [max keys]            ; Gets current server data summary to guide further queries.
                                                       Displayed keys (method/uri) could be limited (5 by default, -1: no limit).
                   [--clean] [query filters]         ; Removes server data events. Admits additional query filters to narrow the                                                        selection.
Usage: server_data_sequence [-h|--help] [value (available values by default)]; Extract server sequence document from json
                                                                               retrieved in previous server_data() call.
=== Auxiliary ===
Usage: json [-h|--help]; Beautifies previous operation json response content.
            [jq expression, '.' by default]; jq filter over previous content.
            Example filter: schema && json '.[] | select(.id=="myRequestsSchema")'
            Auto-execution: assign non-empty value to 'BEAUTIFY_JSON'.
Usage: trace [-h|--help] [level: Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency]; Gets/sets h2agent tracing level.
Usage: metrics [-h|--help]; Prometheus metrics.
Usage: snapshot [-h|--help]; Creates a snapshot directory with process data & configuration.
Usage: server_example [-h|--help]; Basic server configuration examples. Try: source <(server_example)
Usage: help; This help. Overview: help | grep ^Usage
```

### OAM

You could use any visualization framework to analyze metrics information from `h2agent` but perhaps the simplest way to do it is using the `metrics` function  (just a direct `curl` command to the scrape port) from [function helpers](#helper-functions). For example, after *component test* execution this could be the metrics snapshot obtained:

```bash
$ kubectl exec -it -n ns-ct-h2agent h2agent-55b9bd8d4d-2hj9z -- sh -c "apk add curl && curl http://localhost:8080/metrics"

fetch https://dl-cdn.alpinelinux.org/alpine/v3.14/main/x86_64/APKINDEX.tar.gz
fetch https://dl-cdn.alpinelinux.org/alpine/v3.14/community/x86_64/APKINDEX.tar.gz
(1/5) Installing ca-certificates (20191127-r5)
(2/5) Installing brotli-libs (1.0.9-r5)
(3/5) Installing nghttp2-libs (1.43.0-r0)
(4/5) Installing libcurl (7.79.1-r0)
(5/5) Installing curl (7.79.1-r0)
Executing busybox-1.33.1-r3.trigger
Executing ca-certificates-20191127-r5.trigger
OK: 8 MiB in 19 packages
# HELP exposer_transferred_bytes_total Transferred bytes to metrics services
# TYPE exposer_transferred_bytes_total counter
exposer_transferred_bytes_total 0
# HELP exposer_scrapes_total Number of times metrics were scraped
# TYPE exposer_scrapes_total counter
exposer_scrapes_total 0
# HELP exposer_request_latencies Latencies of serving scrape requests, in microseconds
# TYPE exposer_request_latencies summary
exposer_request_latencies_count 0
exposer_request_latencies_sum 0
exposer_request_latencies{quantile="0.5"} Nan
exposer_request_latencies{quantile="0.9"} Nan
exposer_request_latencies{quantile="0.99"} Nan
# HELP AdminHttp2Server_observed_requests_total Http2 total requests observed in AdminHttp2Server
# TYPE AdminHttp2Server_observed_requests_total counter
AdminHttp2Server_observed_requests_total{method="HEAD",success="false"} 0
AdminHttp2Server_observed_requests_total{method="GET",success="false"} 0
AdminHttp2Server_observed_requests_total{method="other",success="false"} 0
AdminHttp2Server_observed_requests_total{method="PUT",success="false"} 0
AdminHttp2Server_observed_requests_total{method="HEAD"} 0
AdminHttp2Server_observed_requests_total{method="DELETE",success="false"} 0
AdminHttp2Server_observed_requests_total{method="POST",success="false"} 2
AdminHttp2Server_observed_requests_total{method="other"} 0
AdminHttp2Server_observed_requests_total{method="DELETE"} 31
AdminHttp2Server_observed_requests_total{method="PUT"} 6
AdminHttp2Server_observed_requests_total{method="GET"} 23
AdminHttp2Server_observed_requests_total{method="POST"} 77
# HELP MockHttp2Server_observed_requests_total Http2 total requests observed in MockHttp2Server
# TYPE MockHttp2Server_observed_requests_total counter
MockHttp2Server_observed_requests_total{method="HEAD",success="false"} 0
MockHttp2Server_observed_requests_total{method="GET",success="false"} 0
MockHttp2Server_observed_requests_total{method="other",success="false"} 0
MockHttp2Server_observed_requests_total{method="PUT",success="false"} 0
MockHttp2Server_observed_requests_total{method="HEAD"} 0
MockHttp2Server_observed_requests_total{method="DELETE",success="false"} 0
MockHttp2Server_observed_requests_total{method="POST",success="false"} 0
MockHttp2Server_observed_requests_total{method="other"} 0
MockHttp2Server_observed_requests_total{method="DELETE"} 1
MockHttp2Server_observed_requests_total{method="PUT"} 0
MockHttp2Server_observed_requests_total{method="GET"} 30
MockHttp2Server_observed_requests_total{method="POST"} 25
# HELP h2agent_observed_requests_total Http2 total requests observed in h2agent
# TYPE h2agent_observed_requests_total counter
h2agent_observed_requests_total{result="unprovisioned"} 7
h2agent_observed_requests_total{result="processed"} 49
# HELP h2agent_purged_contexts_total Total contexts purged in h2agent
# TYPE h2agent_purged_contexts_total counter
h2agent_purged_contexts_total{result="failed"} 0
h2agent_purged_contexts_total{result="successful"} 1
# HELP AdminHttp2Server_responses_delay_seconds_gauge Http2 message responses delay gauge (seconds) in AdminHttp2Server
# TYPE AdminHttp2Server_responses_delay_seconds_gauge gauge
AdminHttp2Server_responses_delay_seconds_gauge 0.000581
# HELP AdminHttp2Server_messages_size_bytes_gauge Http2 message sizes gauge (bytes) in AdminHttp2Server
# TYPE AdminHttp2Server_messages_size_bytes_gauge gauge
AdminHttp2Server_messages_size_bytes_gauge{direction="tx"} 103
AdminHttp2Server_messages_size_bytes_gauge{direction="rx"} 503
# HELP MockHttp2Server_responses_delay_seconds_gauge Http2 message responses delay gauge (seconds) in MockHttp2Server
# TYPE MockHttp2Server_responses_delay_seconds_gauge gauge
MockHttp2Server_responses_delay_seconds_gauge 0.000198
# HELP MockHttp2Server_messages_size_bytes_gauge Http2 message sizes gauge (bytes) in MockHttp2Server
# TYPE MockHttp2Server_messages_size_bytes_gauge gauge
MockHttp2Server_messages_size_bytes_gauge{direction="tx"} 53
MockHttp2Server_messages_size_bytes_gauge{direction="rx"} 0
# HELP AdminHttp2Server_responses_delay_seconds_histogram Http2 message responses delay (seconds) in AdminHttp2Server
# TYPE AdminHttp2Server_responses_delay_seconds_histogram histogram
AdminHttp2Server_responses_delay_seconds_histogram_count 137
AdminHttp2Server_responses_delay_seconds_histogram_sum 0.09854099999999998
AdminHttp2Server_responses_delay_seconds_histogram_bucket{le="+Inf"} 137
# HELP AdminHttp2Server_messages_size_bytes_histogram Http2 message sizes (bytes) in AdminHttp2Server
# TYPE AdminHttp2Server_messages_size_bytes_histogram histogram
AdminHttp2Server_messages_size_bytes_histogram_count{direction="tx"} 137
AdminHttp2Server_messages_size_bytes_histogram_sum{direction="tx"} 11869
AdminHttp2Server_messages_size_bytes_histogram_bucket{direction="tx",le="+Inf"} 137
AdminHttp2Server_messages_size_bytes_histogram_count{direction="rx"} 137
AdminHttp2Server_messages_size_bytes_histogram_sum{direction="rx"} 17259
AdminHttp2Server_messages_size_bytes_histogram_bucket{direction="rx",le="+Inf"} 137
# HELP MockHttp2Server_responses_delay_seconds_histogram Http2 message responses delay (seconds) in MockHttp2Server
# TYPE MockHttp2Server_responses_delay_seconds_histogram histogram
MockHttp2Server_responses_delay_seconds_histogram_count 56
MockHttp2Server_responses_delay_seconds_histogram_sum 0.02644799999999999
MockHttp2Server_responses_delay_seconds_histogram_bucket{le="+Inf"} 56
# HELP MockHttp2Server_messages_size_bytes_histogram Http2 message sizes (bytes) in MockHttp2Server
# TYPE MockHttp2Server_messages_size_bytes_histogram histogram
MockHttp2Server_messages_size_bytes_histogram_count{direction="tx"} 56
MockHttp2Server_messages_size_bytes_histogram_sum{direction="tx"} 1930
MockHttp2Server_messages_size_bytes_histogram_bucket{direction="tx",le="+Inf"} 56
MockHttp2Server_messages_size_bytes_histogram_count{direction="rx"} 56
MockHttp2Server_messages_size_bytes_histogram_sum{direction="rx"} 796
MockHttp2Server_messages_size_bytes_histogram_bucket{direction="rx",le="+Inf"} 56
```

 So, metrics implemented could be divided in two categories, **counters** and **gauges/histograms**. Note that interface type is separated to better understand (i.e. `AdminHttp2Server_observed_requests_total` vs `MockHttp2Server_observed_requests_total`):

#### Counters

- Processed requests
- Non provisioned requests
- Purged contexts (successful/failed)
- POST requests
- GET requests
- PUT requests
- DELETE requests
- HEAD requests
- <other> requests
- Error-condition requests (POST/GET/PUT/DELETE/HEAD/other)

#### Gauges and histograms

- Response delay seconds
- Message size bytes for receptions (Rx)
- Message size bytes for transmissions (Tx)

## Contributing

Check the project [contributing guidelines](./CONTRIBUTING.md).
