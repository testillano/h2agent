# Rate My Service

You have a service that claims to handle requests at a certain rate. But does it really? In this kata you will use `h2agent` client mode to measure the actual throughput of a server and verify it matches the configured rate.

## Client provision dynamics

When triggering a client provision, you can control the rate and total count:

```
GET /admin/v1/client-provision/<id>?sequenceBegin=1&sequenceEnd=50&cps=10
```

This fires the provision 50 times at 10 provisions per second (~5 seconds total). You can poll the progress at any time via the REST API:

```bash
curl -s --http2-prior-knowledge http://localhost:8074/admin/v1/client-provision | jq '.[0].dynamics'
```

Which returns:

```json
{
  "sequence": 47,
  "sequenceBegin": 1,
  "sequenceEnd": 50,
  "cps": 10,
  "repeat": false
}
```

The timer is done when `sequence > sequenceEnd`.

## Let's play

The server and client provisions are already configured. Just run `test.sh` to watch `h2agent` trigger 50 provisions at 5 cps and measure the actual throughput. No files to create — sit back and enjoy the numbers!
