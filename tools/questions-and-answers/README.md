# Q&A Play

Install requirements file by mean:

```bash
$ pip3 install -r requirements.txt
```

And then, execute the python script:

```bash
$ python3.9 tools/questions-and-answers/run.py
```

Please note that the conversational bot has certain limitations and although it relies on the documentation of this project, some questions may be incorrect or incomplete. It is always recommended to carefully read all the documentation, especially the main [README.md](../../README.md) file.

You may use this python script within the training docker image, but all the dependencies must be installed there (a prompt when executing `./tools/training.sh`, ask for it because those dependencies are quite big and the user could prefer to run natively if those requirements are already installed outside).
