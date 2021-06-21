# Manual native testing

## Content

```
.
├── README
├── schemas.sh
├── server-matching
│   ├── FullMatching_ignore_qparams.json
│   ├── FullMatching_passby_qparams.json
│   ├── FullMatching_sort_qparams.json
│   └── FullMatching_sortsemicolon_qparams.json
├── ssl
│   ├── create_all.sh
│   └── create_self-signed_certificate.sh
├── test.sh
└── tests
    ├── default_GET_provision
    ├── get_app_v1_foo_bar_1-name_test.no-filter-generalRandom
    ├── get_app_v1_foo_bar_1-name_test.no-filter-generalRecvseq
    ├── get_app_v1_foo_bar_1-name_test.no-filter-generalStrftime
    ├── get_app_v1_foo_bar_1-name_test.no-filter-generalTimestamp-ns
    ├── get_app_v1_foo_bar_1-name_test.no-filter-inState
    .
    .
    ├── get_app_v1_foo_bar_1.filter-Sum
    ├── get_app_v1_foo_bar_1.mixed-transformations-with-filters
    └── get_app_v1_foo_bar_1.no-transformation
```

## Description

* README: this readme file
* schemas.sh: shows all the schemas available (also requests schema if configured).
* server-matching: server matching configurations in json format.
* ssl: utilities to create certificates and test the server with tls/ssl enabled.
* test.sh: manual test script helper which manages server matching configuration (stored at ./server-matching) as well as server provisioning and traffic tests.
* tests: each file within './tests' have three sections delimited with the tags: 'PROVISION', 'REQUEST_BODY' and 'REQUEST_HDRS'. Under them, the corresponding content will be used to provision the agent, and to know if the traffic request has a body and optional headers.
