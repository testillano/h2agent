# Grafana playground

## Monitoring front-end

Here you will find a `docker-compose` automation to start grafana and prometheus (also node-exporter metrics are included) on host network:

```
├── docker-compose.yaml
├── grafana
│   ├── env.config
│   ├── grafana.ini
│   ├── ldap.toml
│   └── provisioning
│       ├── dashboards
│       │   ├── dashboard.yaml
│       │   └── h2agent.json
│       └── datasources
│           └── datasource.yaml
└── prometheus
    └── prometheus.yaml
```

As you may know, you can start font-end services with:

```bash
$> docker-compose up -d
```

Stop them by mean:

```bash
$> docker-compose down -v
```

And finally, access grafana local server site on browser (authentication is hardcoded to `admin`/`admin1`, so change it at `grafana/env.config` if needed):

```bash
$> firefox http://localhost:9000
```

There, you will find the `h2agent` pre-configured dashboard, in order to observe any host deployment available:

## Use case

To excite the monitoring system, we have prepared a simple use case involving `h2agent` main process and also `udp-client` and `udp-server-h2agent`:

```bash
$> source use-case.src
```

It simulates some customers requesting books to a book store. When the book exists, they buy it and the system annotates the book information through a logging system. Those requests will be `GET /buy/book/<request-order>`, and request order will be a monotonically increasing sequence. The book store was recently created so very few books are available. Assume for example that only 2% of requests are positive. We will program this by mean `mod` operation (`sequence % 100`) as the book identifier, and matching only two of the values in range `0-99`, for example 81 and 99, corresponding for example to these two books:

```
81: Human Action (Von Mises, Ludwig)
99: The Road To Serfdom (Hayek, F.A.)
```

The `GET` response to the customer, will contain the matched book information and status code 200. If the book is missing, status code will be 202 and a detailed message to apologyze will be returned back.

When found, the book store will generate an `UDP`datagram to infer a `POST /sold/book/<book id>` request from `udp-server-h2client` to itself again, and will be answered with status code 201. That request represent the journal entry of the sold book, and a log register (`sales.log`) must be created with a content like this:

```
Book: Human Action | Author: Von Mises, Ludwig | Cost: 51 euro
```

The rest of `GET` requests for missing books will log a register (`lost_sales.log`) like this:

```
Request order: 1245581
```

The reason to use UDP processes to simulate the customer (which is basically a system to launch HTTP/2 requests) and also the `POST` requests when a book is found and sold, is because `h2agent` client mock is being developed, and only functional mode is available. So, to launch a sequence of requests, we need to drive them via UDP datagrams written by `udp-client`, and read by `udp-server-h2client` which also transform them into HTTP/2 requests towards `h2agent`.

With the setup described, we will cover metrics for `h2agent` traffic server endpoint, file system module (logs written), socket manager module (UDP datagram to provoke POST requests), and `udp-server-h2client` metrics for its traffic client endpoint. Pay attention to source labels to see from which process come the metrics in each case:

```

    CUSTOMER                                             BOOK STORE               LOGGING SYSTEM
   ┌─────────────────────────────────────────────┐      ┌───────────┐            ┌───────────────┐
   │ ┌──────────┐          ┌───────────────────┐ │      │ ┌───────┐ │   write    │ ┌───────────┐ │
   │ │UDP-client│          │UDP-server h2client│ │      │ │h2agent│ │ ─────────> │ │File System│ │
   │ └────┬─────┘          └─────────┬─────────┘ │      │ └───┬───┘ │            │ └───────────┘ │
   │      │       udp datagram       │       udp datagram     │     │            └───────────────┘
   │      │ ────────────────────────>│<───────────────────────│     │
   │      │                          │           │      │     │     │
   │      │                          │          req     │     │     │
   │      │                          │───────────────────────>│     │
   │      │                          │           │      │     │     │
   │      │                          │          res     │     │     │
   │      │                          │<───────────────────────│     │
   │ ┌────┴─────┐          ┌─────────┴─────────┐ │      │ ┌───┴───┐ │
   │ │UDP-client│          │UDP-server h2client│ │      │ │h2agent│ │
   │ └──────────┘          └───────────────────┘ │      │ └───────┘ │
   └─────────────────────────────────────────────┘      └───────────┘

```

Enjoy playing !
