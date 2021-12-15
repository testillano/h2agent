# Matching algorithms I: FullMatching

**(It is recommended to read more about matching algorithms at project [documentation](https://github.com/testillano/h2agent#post-adminv1server-matching)**)

One of the most important `h2agent` features consists in the traffic classification needed to select the desired provision where the mock behavior is configured.

That classification filters the `URIs` received by mean an specific algorithm. The simplest one is the `FullMatching` transformation which basically does **nothing**:

```
{
  "algorithm": "FullMatching"
}
```

That is to say, the `URI` received will be used <u>directly to search the corresponding provision</u>. So, for example, if a `POST` request on `/one/uri/path` is received, the following provision is selected to react to the incoming message:

```
{
  "requestMethod": "POST",
  "requestUri": "/one/uri/path",
  "responseCode": 200
}
```

There are more possibilities covered in the matching configuration schema for full matching algorithm. For example, the way to process possible query parameters. So, you can *sort* them by ampersand (which is the default) or semicolon, you could *pass by* the whole `URI` without sorting, and of course, you could ignore them.

For example, the previous provision would be never reached if the `URI` is like `/one/uri/path?name=foo`.

If that is the case, just configure the matching algorithm to ignore the query parameters when they exist:

```
{
  "algorithm": "FullMatching",
  "uriPathQueryParametersFilter": "Ignore"
}
```

This does not mean that you won't have access to query parameters within provision configuration: that's only an specification for traffic classification.

## Exercise

Complete the `matching.json` file, in order to receive an status code `200` for a `GET` request with the `URI`: `/one/uri/path?name=Hayek&city=Friburgo`.

It is not allowed to modify the `provision.json` file.

Note that the `h2agent` answers with status code 501 when a provision is not found for the incoming reception.