# Tools

## Content

```
.
├── README.md
├── common.src
├── coverage.sh
├── grafana
│   ├── PLAY_GRAFANA.md
│   ├── docker-compose.yaml
│   ├── grafana
│   │   ├── env.config
│   │   ├── grafana.ini
│   │   ├── ldap.toml
│   │   └── provisioning
│   │       ├── dashboards
│   │       │   ├── dashboard.yaml
│   │       │   └── h2agent.json
│   │       └── datasources
│   │           └── datasource.yaml
│   ├── prometheus
│   │   └── prometheus.yaml
│   └── use-case.src
├── play-h2agent
│   ├── examples
│   │   ├── Evolution
│   │   │   ├── readme.txt
│   │   │   ├── request.json
│   │   │   ├── server-matching.json
│   │   │   └── server-provision.json
│   │   ├── ExtractTrailingNumberInURI
│   │   │   ├── readme.txt
│   │   │   ├── request.json
│   │   │   ├── server-matching.json
│   │   │   └── server-provision.json
.   .   .
.   .   .
│   └── play.sh
├── helpers.src
├── matching-helper
│   ├── CMakeLists.txt
│   ├── Makefile
│   └── main.cpp
├── arashpartow-helper
│   ├── CMakeLists.txt
│   ├── Makefile
│   └── main.cpp
├── udp-server
│   ├── CMakeLists.txt
│   ├── Makefile
│   └── main.cpp
├── udp-server-h2client
│   ├── CMakeLists.txt
│   ├── Makefile
│   └── main.cpp
├── udp-client
│   ├── CMakeLists.txt
│   ├── Makefile
│   └── main.cpp
├── schemas.sh
├── ssl
│   ├── create_all.sh
│   └── create_self-signed_certificate.sh
├── training.sh
├── url.sh
└── valgrind.sh
```

## Description

* README.md: this readme file
* common.src: common functions used in `./demo` and `./kata`.
* coverage.sh: automated coverage report based in unit tests.
* helpers.src: helper functions to administrate and inspect the `h2agent` application.
* play-h2agent: examples and a guide through them (`play.sh`).
* matching-helper: c++ utility to test regular expressions as a configuration helper.
* arashpartow-helper: c++ utility to test Arash-Partow math expressions.
* udp-server: c++ utility to test UDP messages written by `h2agent` by mean `UDPSocket` target (or any other process writting the socket).
* udp-server-h2client: c++ utility which acts as a udp-server that also triggers requests towards HTTP/2 server.
* udp-client: c++ utility to generate UDP datagrams.
* grafana: docker-compose setup for grafana monitoring. Check `PLAY_GRAFANA.md` as an example to play with it.
* schemas.sh: shows all the schemas available (also requests schema if configured).
* ssl: utilities to create certificates and test the server with tls/ssl enabled.
* training.sh: helper to run training docker image.
* url.sh: helper to encode/decode URLs.
* valgrind.sh: helper to launch the application using valgrind.
