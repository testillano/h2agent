# H2Agent Kata

## Prepare the environment

Check project [README.md](../README.md#Prepare-the-environment).

## Content

Under `./kata` directory you will find a problem for every subdirectory available. Inside each one, a `README.md` file contains the introduction/explanation for the specific problem and the question statement.
Solutions are encrypted (use `./kata/expose_solutions.sh` to decrypt). You could ask the teacher for the password, but... try to solve everything on your own !

Exercise categories:

- `server`: exercises focused on server-side provisions and matching algorithms.
- `client`: exercises focused on client-side provisions and transforms.
- `client+server`: exercises requiring both client and server provisions.
- `break`: fun demos with no missing files to complete — just run and enjoy!

Exercises documentation links:

[1 (server) Matching Algorithms I (FullMatching)](./01.server.Matching_Algorithms_I____FullMatching/README.md)

[2 (server) Matching Algorithms II (FullMatchingRegexReplace)](./02.server.Matching_Algorithms_II___FullMatchingRegexReplace/README.md)

[3 (server) Matching Algorithms III (RegexMatching)](./03.server.Matching_Algorithms_III__RegexMatching/README.md)

[4 (server) Capture Regular Expression](./04.server.Capture_Regular_Expression/README.md)

[5 (server) The Anarchy Of States](./05.server.The_Anarchy_Of_States/README.md)

[6 (break) Rock Paper Scissors](./06.break.Rock_Paper_Scissors/README.md)

[7 (server) Only Those Who Wander Find New Paths](./07.server.Only_Those_Who_Wander_Find_New_Paths/README.md)

[8 (server) Server Data History](./08.server.Server_Data_History/README.md)

[9 (server) Arithmetic Server](./09.server.Arithmetic_Server/README.md)

[10 (server) Foreign States](./10.server.Foreign_States/README.md)

[11 (client) Dynamic Request Body](./11.client.Dynamic_Request_Body/README.md)

[12 (break) Rate My Service](./12.break.Rate_My_Service/README.md)

[13 (client+server) The Forgetful Server](./13.client+server.The_Forgetful_Server/README.md)

[14 (client+server) Knock Knock](./14.client+server.Knock_Knock/README.md)

[15 (client) The Timestamp Oracle](./15.client.The_Timestamp_Oracle/README.md)

## How it works

Depending on the exercise category, the student must create or complete the missing files:

- `server` exercises: create `server-matching.json` and/or `server-provision.json`.
- `client` exercises: complete `client-provision.json`.
- `client+server` exercises: create `server-provision.json` and/or `client-provision.json` (check each exercise statement).
- `break` exercises: nothing to do — just run `test.sh` and have fun!

The testing procedure is always the same: run `test.sh` inside the exercise directory. It configures the agent, sends the required requests and validates the responses.

## Run the kata: evaluation

You could get feedback to know if your current answers are correct just executing `./kata/evaluate.sh`.

## Maintenance (kata administrators)

This section is for whoever maintains the exercises, not for students.

### How solutions are kept

Each solved file is stored encrypted next to its exercise version: `<file>.enc` holds the solution of `<file>` (AES-256-CBC, `openssl enc -pbkdf2`), and `SOLUTION.md.enc` the explanation. The plain files committed are the starting points the student sees. The password is kept in the private repository `testillano/h2agent-passwords`.

A file without a `.enc` partner (for example the server provision of a `client` exercise, or every `test.sh`) is not a solution: it is committed in plain text.

### Expose, fix, hide

```bash
$ cd kata
$ ./expose_solutions.sh <password>   # decrypts every <file>.enc into <file> (overwrites the starting points)
$                                    # also activates the pre-commit hook (core.hooksPath kata/.githooks)
$ NON_INTERACT=true ./evaluate.sh    # option 'a': every exercise must pass with the solutions exposed
$ # ... fix solutions, SOLUTION.md, test.sh, README.md ...
$ ./hide_solutions.sh <password>     # encrypts again every <file> having a <file>.enc partner
```

Then restore the starting points and commit:

```bash
$ git checkout -- $(for e in $(git ls-files '*.enc'); do f=${e%.enc}; git ls-files --error-unmatch "$f" &>/dev/null && echo "$f"; done)
$ git add $(git ls-files -m -o --exclude-standard -- '*.enc') <plain files changed: test.sh, non-solution provisions, README.md>
$ git status   # clean: exposed solutions without a starting point are ignored (kata/.gitignore)
```

The pre-commit hook rejects a commit staging an exposed solution (a file having a `.enc` partner). Exposed solutions without a starting point committed (the files the student must create, and every `SOLUTION.md`) are also listed in `kata/.gitignore`, so they cannot be added by mistake.

### Adding a solution file

To encrypt a new solution file (a new exercise, or a file of an existing one which was not encrypted before), create its empty partner before hiding: `touch <file>.enc`. `hide_solutions.sh` only encrypts files with a `.enc` partner. When the student has to create that file (no starting point committed), also add it to `kata/.gitignore`.

### Evaluation environment

`evaluate.sh` and every `test.sh` use the agent at `localhost:8074` (admin) and `localhost:8000` (traffic); `client` exercises also point their client endpoint to `localhost:8000`. Make sure the agent listening there is the version under test (`curl --http2-prior-knowledge http://localhost:8074/admin/v2/capabilities`), restart it between runs if needed, and that no other process holds those ports.

### When the provision grammar or the admin API change

Migrate both the starting points and the exposed solutions (`h2agent-migrate --in <file> --out <file>`, and `h2agent-migrate --scan .` must report nothing), update `SOLUTION.md` texts, the `README.md` statements and the `test.sh` admin calls, run the evaluation and hide again.

## Good luck

Remember that reading the whole project documentation carefully could help you to make the adventure more profitable.

Anyway, enable debugging traces to the executable if you are completely lost (just add `-l Debug` to verbose output), but take into account that those traces could be useful or not as they are not intended to solve this kind of exercises.

Remember to source `./tools/helpers.bash` which is a very good set of functions to better troubleshoot.

Thank you and enjoy !
