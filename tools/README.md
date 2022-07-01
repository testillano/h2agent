# Tools

## Content

```
.
├── README.md
├── common.src
├── coverage.sh
├── helpers.src
├── manual
│   ├── server-matching
│   │   ├── FullMatching_ignore_qparams.json
│   │   ├── FullMatching_passby_qparams.json
│   │   ├── FullMatching_sort_qparams.json
│   │   └── FullMatching_sortsemicolon_qparams.json
│   ├── test.sh
│   └── tests
│       ├── default_GET_provision
│       ├── get_app_v1_foo_bar_1-name_test.no-filter-eventBody
│       ├── get_app_v1_foo_bar_1-name_test.no-filter-generalRandom
│       ├── get_app_v1_foo_bar_1-name_test.no-filter-generalRecvseq
.       .
.       .
│       ├── get_app_v1_foo_bar_1.mixed-transformations-with-filters
│       └── get_app_v1_foo_bar_1.no-transformation
├── matching-helper
│   ├── CMakeLists.txt
│   ├── Makefile
│   └── main.cpp
├── play-grafana.sh
├── schemas.sh
├── ssl
│   ├── create_all.sh
│   └── create_self-signed_certificate.sh
├── training.sh
└── valgrind.sh
```

## Description

* README.md: this readme file
* common.src: common functions used in `./demo` and `./kata`.
* coverage.sh: automated coverage report based in unit tests.
* helpers.src: helper functions to administrate and inspect the `h2agent` application.
* manual: quick test-bed environment:
  * server-matching: server matching configurations in `json` format.
  * test.sh: manual test script helper which manages server matching configuration (stored at ./server-matching) as well as server provisioning and traffic tests.
  * tests: each file within './tests' have three sections delimited with the tags: 'PROVISION', 'REQUEST_BODY' and 'REQUEST_HDRS'. Under them, the corresponding content will be used to provision the agent, and to know if the traffic request has a body and optional headers.
* matching-helper: c++ utility to test regular expressions as a configuration helper.
* play-grafana.sh: prometheus server and grafana deployment to provide an `h2agent` metrics front-end.
* schemas.sh: shows all the schemas available (also requests schema if configured).
* ssl: utilities to create certificates and test the server with tls/ssl enabled.
* training.sh: helper to run training docker image.
* valgrind.sh: helper to launch the application using valgrind.
