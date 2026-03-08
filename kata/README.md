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
- `client+server` exercises: create `server-provision.json` (client files are provided).
- `break` exercises: nothing to do — just run `test.sh` and have fun!

The testing procedure is always the same: run `test.sh` inside the exercise directory. It configures the agent, sends the required requests and validates the responses.

## Run the kata: evaluation

You could get feedback to know if your current answers are correct just executing `./kata/evaluate.sh`.

## Good luck

Remember that reading the whole project documentation carefully could help you to make the adventure more profitable.

Anyway, enable debugging traces to the executable if you are completely lost (just add `-l Debug` to verbose output), but take into account that those traces could be useful or not as they are not intended to solve this kind of exercises.

Remember to source `./tools/helpers.bash` which is a very good set of functions to better troubleshoot.

Thank you and enjoy !
