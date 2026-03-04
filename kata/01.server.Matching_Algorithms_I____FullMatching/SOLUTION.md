Matching algorithm, by default, <u>sorts</u> query parameters, then **"/one/uri/path?name=Hayek&city=Friburgo"** turns into **"/one/uri/path?city=Friburgo&name=Hayek"** which is not strictly provisioned.

You need to specify `'PassBy'` for `'uriPathQueryParametersFilter'` to respect the original URI received:

```json
{
  "algorithm": "FullMatching",
  "uriPathQueryParameters": {
    "filter": "PassBy"
  }
}
```

Another solution would be provision the ordered version of the URL, but this exercise didn't allow to update that configuration.
