# Q&A Play

Install requirements file by mean:

```bash
$ pip3 install -r tools/questions-and-answers/requirements.txt
```

And then, execute the python script:

```bash
$ python3 tools/questions-and-answers/run.py
```

Please note that the conversational bot has certain limitations and although it relies on the documentation of this project, some questions may be incorrect or incomplete. It is always recommended to carefully read all the documentation, especially the main [README.md](../../README.md) file.

## Using training image

You may use this python script within the training docker image, but all the dependencies must be installed there (a prompt when executing `./tools/training.sh` script will ask for it because those dependencies are quite big and the user could prefer to run natively if those requirements are already installed in the host). Remember that you will need to activate the virtual python environment inside the training image to use `run.py` python script:

````bash
root@998063cf6a4f:/home/h2agent# source /opt/venv/bin/activate
(venv) root@998063cf6a4f:/home/h2agent# python3 tools/questions-and-answers/run.py
Creating vectorstore ...

Ask me anything (0 = quit): What is a transformation item within h2agent process configuration ?
In the context of h2agent process configuration, a transformation item is a configuration object that defines a transformation to be applied to a message received by h2agent. The transformation item specifies the source of the data to be transformed, the target location for the transformed data, and the transformation to be applied.

For example, the following transformation item extracts a random choice of rock, paper, or scissors from the `randomset` variable and sets it as the response body:
```json
{
  "source": "randomset.rock|paper|scissors",
  "target": "response.body.json.string"
}
```
Transformation items are specified in the `transform` array of a provision configuration object. They allow for advanced configurations, such as extracting information from the message received (body, headers, URI, etc.), modifying them, and then transferring the modified data to another location.

Ask me anything (0 = quit):
````

Also remember that **environment variables** `OPENAI_API_KEY` and/or `GROQ_API_KEY` must be defined (the `openai api key` is mandatory as it is used at least the first time to create the vector store for embeddings. After that, if `groq api key` is defined, it will be used for querying the model because it is free/cheaper compared to `openAI`). Those variables are exported from calling shell when running the training docker image.
