# Kata exercises

## Content

Here under `./kata` directory you will find a problem for every subdirectory available. Inside each one, a `README.md` file contains the introduction/explanation for the specific problem and the question statement.
Solutions are available at git branch `kata-solutions`, but ... try to solve everything by your own before checkout !

You could get feedback to know if your current answers are correct just executing `./kata/evaluate.sh`:

```bash
$ ./evaluate.sh -h

Usage: ./evaluate.sh [directory to check, all by default]

       Prepend variables:

       INTERACT: non-empty value exposes interactive inputs. Disabled by default.
```

## How it works

The student must complete each test directory with the corresponding missing stuff: `matching.json` or `provision.json` are normally required. The testing procedure is always the same:

1. The agent is configured with `matching.json` to set the matching algorithm.
2. The agent is configured with `provision.json` to set its reaction behavior.
2. An HTTP/2 request (provided by the problem) is sent to the agent.
3. The answer received from the agent is validated to check if the solution is correct.

## Requirements

This evaluation Kata requires `curl`, `jq` and `dos2unix`, so please try to install them on your system. For example:

```bash
$ sudo apt-get install curl
$ sudo apt-get install jq
$ sudo apt-get install dos2unix
```

## Get started

Don't forget to start the `h2agent` in a separate terminal. You will need to build the application before:

```bash
$ ./build.sh --auto # builds agent
$ build/Release/bin/h2agent --verbose # starts agent
```

You could even enable debugging traces if you are completely lost (just add `-l Debug`), but take into account that those traces could be useful or not as they are not intended to solve this kind of exercises.

If you manage to do real nonsenses, it is possible that you cause a crash of the application, in which case, don't hesitate to report it [here](https://github.com/testillano/h2agent/issues/new?assignees=&labels=&template=bug_report.md&title=Crash).

Thank you and enjoy !