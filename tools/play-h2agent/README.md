# H2Agent Play

## Prepare the environment

### Working in training container

If your are working in the training container (check the training section in the main project [README.md](../README.md) file), there is no need to build the project neither install requirements commented below, just execute the process in background:

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

Some utilities may be required (mainly `curl`, `jq` and `dos2unix`), so please try to install them on your system. For example:

```bash
$ sudo apt-get install curl
$ sudo apt-get install jq
$ sudo apt-get install dos2unix
```

Also, you should build the project and start the `h2agent`:

```bash
$ ./build.sh --auto # builds project images
$ ./h2a.sh --verbose # starts agent with docker by mean helper script
```

You may also build the executable natively:

```bash
$> ./build-native.sh # builds executable
$> ./build/Release/bin/h2agent --verbose # starts executable
```

## Start to play

Once `h2agent` process is [started](#Prepare-the-environment), run the script (use another terminal if needed):

```bash
$> tool/play-h2agent/play.sh
```

Although this is a moderate set of examples, it will be expanded in the future as new exercises are considered useful to improve understanding of the process. The idea is to present use cases that are as practical and common as possible to avoid overwhelming the reader with information given the immense flexibility of the system and its possibilities.

