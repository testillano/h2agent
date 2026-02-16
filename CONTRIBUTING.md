# C++ HTTP/2 Mock Service Contribution Guidelines

If you want to contribute the project with fixes or new features, you must follow these steps:

## Create issue(s)

In case that you detect a crash in the application, report it [here](https://github.com/testillano/h2agent/issues/new?assignees=&labels=&template=bug_report.md&title=Crash).

To share ideas and suggest new features, report [here](https://github.com/testillano/h2agent/issues/new?assignees=&labels=&template=feature_request.md&title=idea).

## Fork and do the change(s)

Fork the project into your `github` account and make all the needed changes. You could consider create a new branch, but working in `master` is good enough. If you have already a fork for this project, just `rebase` master to update the latest changes.

## Check the change(s)

Check that everything is fine before submitting the work:

### Unit tests

Run [unit tests](./README.md#unit-test) and check that nothing was broken.

### Component tests

Run [component tests](./README.md#component-test) and check that nothing was broken.

### Additional tests

It would be also good to check the [demo](./README.md#demo) and [benchmark](./README.md#benchmarking-test) health.

### Source style format

Please, execute `astyle` formatting (using [frankwolf image](https://hub.docker.com/r/frankwolf/astyle)):

```bash
$ sources=$(find . -name "*.hpp" -o -name "*.cpp")
$ docker run -i --rm -v $PWD:/data frankwolf/astyle ${sources}
```

*Note*: if your are using `./build.sh` script to build the source, <u>this formatting procedure is automatic</u>.

## Commit and push

Check [here](https://chris.beams.io/posts/git-commit/) for a good reference of universal conventions regarding how to describe commit messages and make changes.

Don't forget to include the trailer(s) referencing the issue(s) `URLs` to allow tracking the changes in the future. For example:

```
Fix wrong read

This change fix a bug when trying to ...
...

Issue: https://github.com/testillano/h2agent/issues/14
```

Finally, push the commit(s) from your local fork towards its repository origin:

```bash
$ git push origin <branch>
```

## Development Workflow (Project Members)

Feature branches workflow:

1. Create a branch: `git checkout -b feature/my-change`
2. Push to origin: `git push -u origin feature/my-change`
3. Open a PR on GitHub for review
4. Squash merge to master when approved

This keeps master clean and allows iteration on the branch without polluting the main history.

## Check project work-flow

Go to `Actions` tab in your fork page and wait for the "green ball" of *Continuous Integration* (`CI`) work-flow execution.

## Pull request

Create the `pull request` in your `github` fork. Follow [instructions](./pull_request_template.md) and wait for final acceptance.

Thank you in advance for your contribution.

## Merging `master` into `kata-solutions`

There is a branch to store the `kata exercises'` solutions which shall be maintained manually by mean merge of `master` changes:

### Ensure `master` is "up to date"

Before merging, make sure you have the latest version of `master`:
```bash
$ git checkout master
$ git pull origin master  # Fetch and update the latest master changes
```

### Switch to `kata-solutions` branch and merge `master`
```bash
$ git checkout kata-solutions
$ git merge master
```

### Overwrite conflicted files with `master`
If conflicts appear, use the following commands:

```bash
$ git checkout master -- .
$ git status
$ git rm -f <some master deleted files must be also deleted here>
$ git add .
$ git commit
```

Check that branches only differ in solutions:

```bash
$ git diff master..kata-solutions
```

And finally, push the branch:

```bash
$ git push origin kata-solutions
```
