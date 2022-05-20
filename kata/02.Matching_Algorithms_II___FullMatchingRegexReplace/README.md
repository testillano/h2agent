# Matching algorithms II: FullMatchingRegexReplace

**(It is recommended to read more about matching algorithms at project [documentation](https://github.com/testillano/h2agent#post-adminv1server-matching)**)

The  `FullMatchingRegexReplace` algorithm acts like the previous `FullMatching` after a `RegexReplace` transformation over the `URI` received.

This algorithm is tremendously powerful and has endless possibilities when it comes to manipulating information. The `h2agent` implements it through the [regex-replace](http://www.cplusplus.com/reference/regex/regex_replace/) C++ function template.

Such function requires a regular expression where normally some capture groups are defined, and then a formatted expression where the final result is built. Things like trimming, cutting or selecting parts are really simple using this methodology.

For example, given the `URI` `/ctrl/v2/id-555112233/ts-1615562841`, you could remove last timestamp with this matching configuration:

```
{
  "algorithm": "FullMatchingRegexReplace",
  "rgx": "(/ctrl/v2/id-[0-9]+)/(ts-[0-9]+)",
  "fmt": "$1"
}
```

This is because the format `$1` represents the first capture group which discards the timestamp part.

## Exercise

Fix the `server-provision.json` file, in order to receive an status code `200` for a `GET` request with the `URI`: `/ctrl/v2/id-555112233/ts-5555555555`.

It is not allowed to modify the `server-matching.json` file.
