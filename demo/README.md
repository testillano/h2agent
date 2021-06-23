# HTTP/2 Server Mock Demo

## Case of use

Our use case consists in a corporate office with a series of workplaces that can be assigned to an employee or be empty. We will simulate the database having each entry the workplace *ID*, a 5-digit phone extension, the assigned employee name and also an optional root node `developer` to indicate this job role if proceed.

These are the **requirements**:

* Serve *GET* requests with URI `office/v2/workplace?id=<id>`, obtaining the information associated to the workplace. Additionally, the date/time of the query received must be added to the response (`time`: "<free date format>"). We will configure the following entries:

  ```{
  id = id-1
  phone = 66453
  name = Jess Glynne
  ______________________________
  id = id-2
  phone = 55643
  name = Bryan Adams
  ______________________________
  id = id-3
  phone = 32459
  name = Phil Collins
  ```

* If requested *id* is not in the database, we will return a document with the unknown *id* requested, a random phone extension between 30000 and 69999, and the employee name `unassigned`. We will ignore the fact that subsequent queries won't be coherent with regarding the phone number for this unassigned registers (assumed just for simplification).

* Identifiers must be validated as `id-<2-digit number>`. If this is not true, an status code *400 (Bad Request)* will be answered, with this response body explaining the reason:

  ```json
  {
    "cause": "invalid workplace id provided, must be in format id-<2-digit number>"
  }
  ```

* Workplaces with **even** identifiers are reserved for developers, so the extra field commented above must be included in the response body root. This will be manually configured for `id-2` but shall be automated for unassigned ones which are even numbers.

* Finally, we will simulate the deletion for existing identifiers (`id-1`, `id-2` and `id-3`), so when *DELETE* request for them are received, subsequent *GET's* must obtain a *404 (Not Found)* status code from then on. No need to protect *DELETE* against invalid or unassigned `id's`.



## Analysis

As we need to identify bad *URI's*, we will use the matching algorithm <u>*PriorityMatchingRegex*</u>, including firstly the known registers, then the "valid but unassigned" ones, and finally a fall back default provision for `GET` method with the *bad request (400)* configuration.

Note that regular expression *URI's*, <u>MUST ESCAPE</u> slashes and question marks:

​	`"\\/office\\/v2\\/workplace\\?id=id-1"`.

This way is also valid:

​	 `(/office/v2/workplace)\\?id=id-1"`.

The optional `developer` node will be implemented through a <u>condition variable transformation filter</u>.

Finally we will use the <u>out-state for foreign method</u> feature to simulate the registers deletion.



## Solution

Check the file [./demo/provisions.json](./provisions.json).

For further understanding you may read the project [README.md](https://github.com/testillano/h2agent/blob/master/README.md) file.



## Run the demo

Build and start the process:

```bash
$> ./build.sh --auto
$> build/Release/bin/h2agent -l Debug --verbose
```

Run the demo in another terminal:

```bash
$> demo/run.sh
```

The demo script is interactive to follow the use case step by step.
