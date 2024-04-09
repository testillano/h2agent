# H2Agent Kata

## Prepare the environment

Check project [README.md](../README.md#Prepare-the-environment).

## Content

Under `./kata` directory you will find a problem for every subdirectory available. Inside each one, a `README.md` file contains the introduction/explanation for the specific problem and the question statement.
Solutions are available at git branch `kata-solutions`, but ... try to solve everything by your own before checkout !

Exercises documentation links:

[1 Matching Algorithms I (FullMatching)](./01.Matching_Algorithms_I____FullMatching/README.md)

[2 Matching Algorithms II (FullMatchingRegexReplace)](./02.Matching_Algorithms_II___FullMatchingRegexReplace/README.md)

[3 Matching Algorithms III (RegexMatching)](./03.Matching_Algorithms_III__RegexMatching/README.md)

[4 Capture Regular Expression](./04.Capture_Regular_Expression/README.md)

[5 The Anarchy Of States](./05.The_Anarchy_Of_States/README.md)

[6 Rock Paper Scissors (Break Time)](./06.Rock_Paper_Scissors__Break_Time/README.md)

[7 Only Those Who Wander Find New Paths](./07.Only_Those_Who_Wander_Find_New_Paths/README.md)

[8 Server Data History](./08.Server_Data_History/README.md)

[9 Arithmetic Server](./09.Arithmetic_Server/README.md)

[10 Foreign States](./10.Foreign_States/README.md)

## How it works

The student must complete each test directory with the corresponding missing stuff: `server-matching.json` or `server-provision.json` are normally required. The testing procedure is always the same:

1. The agent is configured with `server-matching.json` to set the server matching algorithm.
2. The agent is configured with `server-provision.json` to set its server reaction behavior.
2. An HTTP/2 request (provided by the problem) is sent to the agent.
3. The answer received from the agent is validated to check if the solution is correct.

## Run the kata: evaluation

You could get feedback to know if your current answers are correct just executing `./kata/evaluate.sh`.

## Good luck

Remember that reading the whole project documentation carefully could help you to make the adventure more profitable.

Anyway, enable debugging traces to the executable if you are completely lost (just add `-l Debug` to verbose output), but take into account that those traces could be useful or not as they are not intended to solve this kind of exercises.

Remember to source `./tools/helpers.src` which is a very good set of functions to better troubleshoot.

Thank you and enjoy !
