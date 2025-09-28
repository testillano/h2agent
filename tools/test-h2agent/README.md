# H2Agent Tester

## Prepare the environment

Check [README.md](../../README.md#Prepare-the-environment).

It is required to enable the`nghttpx` proxy for both traffic and administrative interfaces, so you could run the agent in this way:

```bash
$ H2AGENT_TRAFFIC_PROXY_PORT=8001 H2AGENT_ADMIN_PROXY_PORT=8075 ./run.sh
```

## Install requirements

```bash
$ pip3 install -r tools/test-h2agent/requirements.txt
```

## Start to test

Once `h2agent` process is [started](#Prepare-the-environment), run the python script (use another terminal if needed):

```bash
$ tools/test-h2agent/run.py
```
