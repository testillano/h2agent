# Q&A Play

## Prepare the environment

### Working in project checkout

#### Requirements

Some utilities may be required, so please try to install them on your system. For example:

```bash
$ sudo apt-get install netcat
$ sudo apt-get install curl
$ sudo apt-get install jq
$ sudo apt-get install dos2unix
```

#### Starting agent

Then you may build project images and start the `h2agent` with its docker image:

```bash
$ ./build.sh --auto # builds project images
$ ./h2a.sh --verbose # starts agent with docker by mean helper script
```

Or build native executable and run it from shell:

```bash
$> ./build-native.sh # builds executable
$> ./build/Release/bin/h2agent --verbose # starts executable
```

### Working in training container

The training image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$> docker pull ghcr.io/testillano/h2agent_training:<tag>
```

Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.

You may also find useful run the training image by mean the helper script `./tools/training.sh`. This script builds and runs an image based in `./Dockerfile.training` which adds the needed resources to run training resources. The image working directory is `/home/h2agent` making the experience like working natively over the git checkout and providing by mean symbolic links, main project executables.

If your are working in the training container, there is no need to build the project neither install requirements commented in previous section, just execute the process in background:

```bash
bash-5.1# ls -lrt
total 12
drwxr-xr-x    5 root     root          4096 Dec 16 20:29 tools
drwxr-xr-x   12 root     root          4096 Dec 16 20:29 kata
drwxr-xr-x    2 root     root          4096 Dec 16 20:29 demo
lrwxrwxrwx    1 root     root            12 Dec 16 20:29 h2agent -> /opt/h2agent
bash-5.1# ./h2agent --verbose &
```

## Start to play

Just execute the python script by mean:

```bash
$> python3 tools/questions-and-answers/run.py
```

Please note that the conversational bot has certain limitations and although it relies on the documentation of this project, some questions may be incorrect or incomplete. It is always recommended to carefully read all the documentation, especially the main [README.md](../../README.md) file.

You may use this python script within the training docker image, but all the dependencies must be installed there (a prompt ask for it because those dependencies are quite big and the user could prefer to run natively if those requirements are already installed outside).
