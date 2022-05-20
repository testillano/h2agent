# Matching algorithms III: PriorityMatchingRegex

**(It is recommended to read more about matching algorithms at project [documentation](https://github.com/testillano/h2agent#post-adminv1server-matching)**)

The  `PriorityMatchingRegex` algorithm acts checking matches for `URIs` received against the regular expressions configured in the provision ordered list. So, not only the match must validate the selection but the provision order is important because the first expression matched is the one used to rule the behavior.

```
{
  "algorithm": "PriorityMatchingRegex"
}
```

## Exercise

Fix the `server-provision.json` file, in order to receive an status code `200` for a `GET` request with the `URI`: `/ctrl/v2/id-555112233/ts-5555555555`.

It is not allowed to modify the `server-matching.json` file.
