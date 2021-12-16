# Kata exercises

## Prepare the environment

### Working in training container

If your are working in the training container (check the training section in the main project [README.md](../README.md) file), there is no need to build the project neither install requirements commented below, just execute the process in background and run the **kata** stuff:

```bash
bash-5.1# ls -lrt
total 12
drwxr-xr-x    5 root     root          4096 Dec 16 20:29 tools
drwxr-xr-x   12 root     root          4096 Dec 16 20:29 kata
drwxr-xr-x    2 root     root          4096 Dec 16 20:29 demo
lrwxrwxrwx    1 root     root            12 Dec 16 20:29 h2agent -> /opt/h2agent
bash-5.1# ./h2agent --verbose &
```

### Working natively: requirements

This demo requires `curl`, `jq` and `dos2unix`, so please try to install them on your system. For example:

```bash
$ sudo apt-get install curl
$ sudo apt-get install jq
$ sudo apt-get install dos2unix
```

Also, you should build the project and start the `h2agent` (better in a separate terminal):

```bash
$ ./build.sh --auto # builds agent
$ build/Release/bin/h2agent --verbose & # starts agent
```

## Content

Under `./kata` directory you will find a problem for every subdirectory available. Inside each one, a `README.md` file contains the introduction/explanation for the specific problem and the question statement.
Solutions are available at git branch `kata-solutions`, but ... try to solve everything by your own before checkout !

## How it works

The student must complete each test directory with the corresponding missing stuff: `matching.json` or `provision.json` are normally required. The testing procedure is always the same:

1. The agent is configured with `matching.json` to set the matching algorithm.
2. The agent is configured with `provision.json` to set its reaction behavior.
2. An HTTP/2 request (provided by the problem) is sent to the agent.
3. The answer received from the agent is validated to check if the solution is correct.

## Run the kata: evaluation

You could get feedback to know if your current answers are correct just executing `./kata/evaluate.sh`.

## Good luck

Remember that reading the whole project documentation carefully could help you to make the adventure more profitable.

Anyway, enable debugging traces to the executable if you are completely lost (just add `-l Debug`), but take into account that those traces could be useful or not as they are not intended to solve this kind of exercises.

Remember to source `./tools/helpers.src` which is a very good set of functions to better troubleshoot.

If you manage to do real nonsenses, it is possible that you cause a crash of the application, in which case, don't hesitate to report it [here](https://github.com/testillano/h2agent/issues/new?assignees=&labels=&template=bug_report.md&title=Crash).

Thank you and enjoy !