# Capture Regular Expression

Until now, our provision examples were quite simple: they didn't use transformation steps, something which is essential to configure advanced provisions, because they allow to extract information from the message received (body, headers, uri, etc.), modify them and then transfer to any other location.

Some of the most versatile and powerful transformations are based in regular expressions. Their typical use is when we need to get information from the `URI` received to put in some place within the response.

We have already used regular expressions when playing with matching algorithms, for example, the `RegexReplace` algorithm. Imagine that we want to build a unique *IPv4* address corresponding to a phone number received in the `URI` after the `id-`. Look at this smart transformation list:

```json
[
  {
    "source": "request.uri.path",
    "filter": {
      "RegexReplace": {
        "rgx": "(/ctrl/v2/id-)([0-9]{9})(/ts-)([0-9]{10})",
        "fmt": "$2"
      }
    },
    "target": "var.phone"
  },
  {
    "source": "var.phone",
    "filter": {
      "RegexReplace": {
        "rgx": "[0-9]+([0-9]{2})([0-9]{2})([0-9]{2})([0-9]{2})",
        "fmt": "$1.$2.$3.$4"
      }
    },
    "target": "var.ipv4"
  }
]
```

There is no need to implement ad-hoc C++ functions within the agent for this kind of job: regular expressions are your friends. Also, all the regular expressions used by the `h2agent` are precompiled and optimized, so there is no handicap when using them.

Another transformation algorithm which uses regular expressions is the `RegexCapture`. Here, the formatted output string does not exist. This algorithm just matches a string against a regular expression, and retrieves the matching sections which can be interpreted as boolean (empty match is false). But, the best way to use this algorithm is introducing capture groups and targeting an output variable which will store the information for every captured group match. For example:

```json
{
  "source": "request.uri.path",
  "target": "var.uri_parts",
  "filter": { "RegexCapture" : "\/api\/v2\/id-([0-9]+)\/category-([a-z]+)" }
}
```

Note that some characters are escaped when they are outside a capture group (slashes).

The way to store multiple values on the variable `uri_parts` is by mean virtual creation of additional variables through suffixing that variable with an ordinal for the captured group position, so, if we receive the `URI` `/api/v2/id-28/category-animal` we will have:

`uri_parts.1` = 28

`uri_parts.2` = animal

Where suffixes 1 and 2 are the first and second captured group respectively for the regular expression applied.

The main (non-suffixed) variable, will store the global match, which can be used as boolean indicator (non-empty string implies `true`):

`uri_parts` = `/api/v2/id-28/category-animal`

## Exercise

Complete the `provision.json` file, in order to receive an status code `200` for a `GET` request with the `URI`: `/number/is/even/<even number>` and a status code `400` for a `GET` with the same `URI` carrying an odd number: `/number/is/even/<odd number>`.

The `matching.json` file is already provided.

Hint: the `ConditionVar` transformation filter is used to condition the transfer from the source to the target depending on the provided variable boolean value. Take a look to the official documentation [here](https://github.com/testillano/h2agent#post-adminv1server-provision).
