# Rock, Paper, Scissors (Break Time)

In order to take a rest, we will afford this section as a simple game to play with random sets. So, relax because sooner or later you will be right with the answer ...

## Random sources in h2agent

Imagine that you want to simulate server delays. You could provision the following document:

```
{
  "requestMethod": "POST",
  "requestUri": "/one/uri/path",
  "responseDelayMs": 30,
  "responseCode": 200
}
```

But in order to be more realistic, response delays should be better modeled with a random value instead of a fixed one (30 milliseconds in the former example).

For that, you could generate **random** integer numbers in a specific range and transfer the value to the `response.delayMs` target type:

```
{
  "requestMethod": "POST",
  "requestUri": "/one/uri/path",
  "responseCode": 200,
  "transform": [
    {
      "source": "random.25.35",
      "target": "response.delayMs"
    }
  ]
}
```

That is an example of transformation filter to create dynamically the response delay in milliseconds for each `POST` request to the `/one/uri/path`. Note that this transformation has priority over fixed configuration which could be done at `responseDelayMs` field by distraction (this is not validated in the provision schema).

Filters are applied with the given order inside the `transform` node array. You could generate *float random numbers* just combining two integer random numbers by mean the dot symbol. There are many ways to do that, for example using variables to load integer and fractional parts and then parsing the result into another variable (which you could use later in further transformations omitted here, for example transferring it into response body or any other location):

```
{
  "requestMethod": "POST",
  "requestUri": "/one/uri/path",
  "responseCode": 200,
  "transform": [
    {
      "source": "random.0.99",
      "target": "var.integer-part"
    },
    {
      "source": "random.000.999",
      "target": "var.float-part"
    },
    {
      "source": "value.@{integer-part}.@{float-part}",
      "target": "var.random-number-in-xx.yyy-format"
    }
  ]
}
```



But there is another random generation method to get a value within a range of <u>fixed labels instead of numeric range</u>: that is the **random set**.

So, for example imagine three possible values: rock, paper and scissors. The source needed will be just:

`randomset.rock|paper|scissors`

Note that we use `randomset` which expects an input in the form `aa|bb|..|zz` instead of `random` which expected `min.max` input (the reason to choose the pipe symbol (`|`) separator is because it is more probable to need including dots inside labels instead of pipes).

## Static vs dynamic random sets

The list input (`rock|paper|scissors`) could even be constructed from variables, as you already know. For example, for a *heads & tails* game, imagine that you have this kind of request  `URI` to get the labels:

 `/give/me/your/choice/within?firstItem=heads&secondItem=tails`

So we would transfer the query parameters (`request.uri.param.<qp key>`) into variables used to generate the random choice:

```json
{
  "requestMethod": "GET",
  "requestUri": "/give/me/your/choice/within",
  "responseCode": 200,
  "transform": [
    {
      "source": "request.uri.param.firstItem",
      "target": "var.label1"
    },
    {
      "source": "request.uri.param.secondItem",
      "target": "var.label2"
    },
    {
      "source": "randomset.@{label1}|@{label2}",
      "target": "response.body.string"
    }
  ]
}
```

To reach this provision instance, your matching algorithm shall be something like:

```json
{
  "algorithm": "FullMatching",
  "uriPathQueryParameters": {
    "filter": "Ignore"
  }
}
```

So, you could play also to `cara o cruz` Spanish version of the former game, just requesting:

 `/give/me/your/choice/within?firstItem=cara&secondItem=cruz`

Indeed, variables parsing have a lot of possibilities but they are out of the purpose of this exercise.



Now, returning to our `rock, paper, scissors` game, the list is static so we have simplified the provision to this one:

```json
{
  "requestMethod": "GET",
  "requestUri": "/rock paper or scissors",
  "responseCode": 200,
  "transform": [
    {
      "source": "randomset.rock|paper|scissors",
      "target": "response.body.string"
    }
  ]
}
```

Then, you just need to request the `GET` request on `URI` `/rock%20paper%20or%20scissors` to get the random choice done by the mock (the response is not a `json` document, just a string with the choice).

Note that we always provision **decoded** `URIs` and although this complication is not needed for the example, it is good to know that it is possible to mess with that.

## Let's play

### h2agent vs h2agent

Evaluate this exercise to see the `h2agent` playing against itself.

### h2agent vs you

Evaluate this exercise interactively (`./evaluate.sh -i`) to play against the `h2agent`. Can you win to the mock ? I'm sure you will do ... sooner or later !
