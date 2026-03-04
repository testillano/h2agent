We need firstly to mirror the source 'request.body' into the target 'response.body.json.object':

```json
{
  "source": "request.body",
  "target": "response.body.json.object"
}
```

Then, we need to store the numbers received, just transfering them into three variables using a non-filter transformation like this:

```json
{
  "source": "request.body./number1",
  "target": "var.number1"
}
```

Same for `number2` and `number3`.

Then, we build a constant expression with source 'value' by mean parsing the former variables and targeting the path '/processed/numbers' as a json string literal:

```json
{
  "source": "value.[ @{number1}, @{number2}, @{number3} ]",
  "target": "response.body.json.jsonstring./processed/numbers"
}
```

And finally, we transfer the header 'app-version' into the response json path required:

```json
{
  "source": "request.header.app-version",
  "target": "response.body.json.string./processed/appVersion"
}
```
