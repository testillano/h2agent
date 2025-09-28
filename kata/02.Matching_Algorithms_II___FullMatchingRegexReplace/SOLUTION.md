As you can see in the regular expression captured:

```json
{
  "algorithm": "FullMatchingRegexReplace",
  "rgx": "(/ctrl/v2/id-[0-9]+)/(ts-[0-9]+)",
  "fmt": "$1"
}
```

The URI will be filtered to `/ctrl/v2/id-<number>` WITHOUT THE FINAL SLASH, because it is outside the captured group ($1).

Also, the request expected is a 'GET', not a 'POST'. So the provision must be fixed in two places:

```json
{
  "requestMethod": "POST", => MUST BE 'GET'
  "requestUri": "/ctrl/v2/id-555112233/", => MUST BE "/ctrl/v2/id-555112233"
  "responseCode": 200
}
```
