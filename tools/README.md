# Tools

## Content

```
.
├── README.md
├── common.src
├── coverage.sh
├── helpers.src
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
├── play-grafana.sh
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
* udp-server: c++ utility to test UDP messages written by `h2agent` by mean `UDPSocket` target.
* play-grafana.sh: prometheus server and grafana deployment to provide an `h2agent` metrics front-end.
* schemas.sh: shows all the schemas available (also requests schema if configured).
* ssl: utilities to create certificates and test the server with tls/ssl enabled.
* training.sh: helper to run training docker image.
* url.sh: helper to encode/decode URLs.
* valgrind.sh: helper to launch the application using valgrind.
