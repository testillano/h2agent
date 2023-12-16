# Arithmetic Server

It is possible to calculate mathematical expressions by mean the data source `math.<expression>`.

The parser/compiler is provided by [Arash Partow's exprtk](https://github.com/ArashPartow/exprtk) third party library. For example, you may implement a simple arithmetic server with this kind of provision:

```json
{
  "requestMethod": "POST",
  "requestUri": "/app/v1/calculate",
  "responseCode": 200,
  "transform": [
    {
      "source": "request.body",
      "target": "var.expression"
    },
    {
      "source": "math.@{expression}",
      "target": "response.body.json.float"
    }
  ]
}
```

Start the process with that provision content as `provision.json` file:

```bash
$ ./build/Release/bin/h2agent --traffic-server-provision provision.json &>/dev/null &
[1] 31456
```

And send simple query like this to do the job:

```bash
$ curl --http2-prior-knowledge -XPOST -d'(1+sqrt(5))/2' http://127.0.0.1:8000/app/v1/calculate
1.618033988749895
```

Check [here](https://github.com/ArashPartow/exprtk/blob/master/readme.txt) for more information about Arash Partow's library.

## Exercise

Complete `server-matching.json` and create `server-provision.json`, in order to receive an status code `200` for a `GET` request with the `URI`: `/calculate/sdpf?a=<number>&b=<number>&c=<number>&x=<number>`, and an integer response body with the result of truncating the second-degree polynomial function for those provided values: `f(x)=ax2+bx+c`.
