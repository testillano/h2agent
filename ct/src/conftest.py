# Keep sorted
import base64
from collections import defaultdict
import glob
from hyper import HTTP20Connection
#import inspect
import json
import logging
import os
import pytest
import re
import xmltodict

#############
# CONSTANTS #
#############

# Endpoints
H2AGENT_ENDPOINT__admin = os.environ['H2AGENT_SERVICE_HOST'] + ':' + os.environ['H2AGENT_SERVICE_PORT_HTTP2_ADMIN']
H2AGENT_ENDPOINT__traffic = os.environ['H2AGENT_SERVICE_HOST'] + ':' + os.environ['H2AGENT_SERVICE_PORT_HTTP2_TRAFFIC']

# Api Path
ADMIN_URI_PREFIX = '/admin/v1/'
ADMIN_MATCHING_URI = ADMIN_URI_PREFIX + 'server-matching'
ADMIN_PROVISION_URI = ADMIN_URI_PREFIX + 'server-provision'
ADMIN_DATA_URI = ADMIN_URI_PREFIX + 'server-data'

# Headers
CONTENT_LENGTH = 'content-length'

# Flow calculation throw h2af (H2AGENT Flow) fixture:
#   flow = h2af.getId()
#
# For sequenced tests, the id is just a monotonically increased number from 1.
# For parallel tests, this id is sequenced from 1 for every worker, then, globally,
#  this is a handicap to manage flow ids (tests ids) for H2AGENT FSM (finite state machine).
# We consider a base multiplier of 10000, so collisions would take place when workers
#  reserves more than 10000 flows. With a 'worst-case' assumption of `5 flows per test case`,
#  you should cover up to 5000 per worker. Anyway, feel free to increase this value,
#  specially if you are thinking in use pytest and H2AGENT Agent for system tests.
FLOW_BASE_MULTIPLIER = 10000

#########
# HOOKS #
#########
#def pytest_runtest_setup(item):
#    print("pytest_runtest_setup")
#def pytest_runtest_logreport(report):
#    print(f'Log Report:{report}')
#def pytest_sessionstart(session):
#    print("pytest_session start")
#def pytest_sessionfinish(session):
#    print("pytest_session finish")

######################
# CLASSES & FIXTURES #
######################

class Sequencer(object):
    def __init__(self, request):
        self.sequence = 0
        self.request = request

    def __wid(self):
        """
        Returns the worker id, or 'master' if not parallel execution is done
        """
        wid = 'master'
        if hasattr(self.request.config, 'slaveinput'):
          wid = self.request.config.slaveinput['slaveid']

        return wid

    def getId(self):
        """
        Returns the next identifier value (monotonically increased in every call)
        """
        self.sequence += 1

        wid = self.__wid()
        if wid == "master":
          return self.sequence

        # Workers are named: wd0, wd1, wd2, etc.
        wid_number = int(re.findall(r'\d+', wid)[0])

        return FLOW_BASE_MULTIPLIER * wid_number + self.sequence



@pytest.fixture(scope='session')
def h2af(request):
  """
  H2AGENT Flow
  """
  return Sequencer(request)



# Logging
class MyLogger:

  # CRITICAL ERROR WARNING INFO DEBUG NOSET
  def setLevelInfo(): logging.getLogger().setLevel(logging.INFO)
  def setLevelDebug(): logging.getLogger().setLevel(logging.DEBUG)

  def error(message): logging.getLogger().error(message)
  def warning(message): logging.getLogger().warning(message)
  def info(message): logging.getLogger().info(message)
  def debug(message): logging.getLogger().debug(message)

@pytest.fixture(scope='session')
def mylogger():
  return MyLogger

MyLogger.logger = logging.getLogger('CT')

# Base64 encoding:
@pytest.fixture(scope='session')
def b64_encode():
  def encode(message):
    message_bytes = message.encode('ascii')
    base64_bytes = base64.b64encode(message_bytes)
    return base64_bytes.decode('ascii')
  return encode

# Base64 decoding:
@pytest.fixture(scope='session')
def b64_decode():
  def decode(base64_message):
    base64_bytes = base64_message.encode('ascii')
    message_bytes = base64.b64decode(base64_bytes)
    return message_bytes.decode('ascii')
  return decode

@pytest.fixture(scope='session')
def saveB64Artifact(b64_decode):
  """
  Decode base64 string provided to disk

  base64_message: base64 encoded string
  file_basename: file basename where to write
  isXML: decoded data corresponds to xml string. In this case, also json equivalente representation is written to disk
  """
  def save_b64_artifact(base64_message, file_basename, isXML = True):

    targetFile = file_basename + ".txt"
    data = b64_decode(base64_message)

    if isXML:
      targetFile = file_basename + ".xml"

    _file = open(targetFile, "w")
    n = _file.write(data)
    _file.close()

    if isXML:
      targetFile = file_basename + ".json"
      xml_dict = xmltodict.parse(data)
      data = json.dumps(xml_dict, indent = 4, sort_keys=True)

      _file = open(targetFile, "w")
      n = _file.write(data)
      _file.close()

  return save_b64_artifact


# HTTP communication:
class RestClient(object):
    """A client helper to perform rest operations: GET, POST.

    Attributes:
        endpoint: server endpoint to make the HTTP2.0 connection
    """

    def __init__(self, endpoint):
        """Return a RestClient object for H2AGENT endpoint."""
        self._endpoint = endpoint
        self._ip = self._endpoint.split(':')[0]
        self._connection = HTTP20Connection(host=self._endpoint)

    def _log_http(self, kind, method, url, body, headers):
        length = len(body) if body else 0
        MyLogger.info(
                '{} {}{} {} headers: {!s} data: {}:{!a}'.format(
                method, self._endpoint, url, kind, headers, length, body))

    def _log_request(self, method, url, body, headers):
        self._log_http('REQUEST', method, url, body, headers)

    def _log_response(self, method, url, response):
        self._log_http(
                'RESPONSE:{}'.format(response["status"]), method, url,
                response["body"], response["headers"])

    #def log_event(self, level, log_msg):
    #    # Log caller function name and formated message
    #    MyLogger.logger.log(level, inspect.getouterframes(inspect.currentframe())[1].function + ': {!a}'.format(log_msg))

    def parse(self, response):
        response_body = response.read(decode_content=True).decode('utf-8')
        if len(response_body) != 0:
          response_body_dict = json.loads(response_body)
        else:
          response_body_dict = ''
        response_data = { "status":response.status, "body":response_body_dict, "headers":response.headers }
        return response_data

    def request(self, requestMethod, requestUrl, requestBody=None, requestHeaders=None):
      """
      Returns response data dictionary with 'status', 'body' and 'headers'
      """
      requestBody = RestClient._pad_body_and_length(requestBody, requestHeaders)
      self._log_request(requestMethod, requestUrl, requestBody, requestHeaders)
      self._connection.request(method=requestMethod, url=requestUrl, body=requestBody, headers=requestHeaders)
      response = self.parse(self._connection.get_response())
      self._log_response(requestMethod, requestUrl, response)
      return response

    def _pad_body_and_length(requestBody, requestHeaders):
        """Pad the body and adjust content-length if needed.
        When the length of the body is multiple of 1024 this function appends
        one space to the body and increases by one the content-length.

        This is a workaround for hyper issue 355 [0].
        The issue has been fixed but it has not been released yet.

        [0]: https://github.com/Lukasa/hyper/issues/355

        EXAMPLE
        >>> body, headers = ' '*1024, { 'content-length':'41' }
        >>> body = RestClient._pad_body_and_length(body, headers)
        >>> ( len(body), headers['content-length'] )
        (1025, '42')
        """
        if requestBody and 0 == (len(requestBody) % 1024):
            logging.warning( "RestClient.request:" +
                             " padding body because" +
                             " its length ({})".format(len(requestBody)) +
                             " is multiple of 1024")
            requestBody += " "
            content_length = CONTENT_LENGTH
            if requestHeaders and content_length in requestHeaders:
                length = int(requestHeaders[content_length])
                requestHeaders[content_length] = str(length+1)
        return requestBody

    def get(self, requestUrl):
        return self.request('GET', requestUrl)

    def put(self, requestUrl, requestBody = None, requestHeaders={'content-type': 'application/json'}):
        return self.request('PUT', requestUrl, requestBody, requestHeaders)

    def post(self, requestUrl, requestBody = None, requestHeaders={'content-type': 'application/json'}):
        return self.request('POST', requestUrl, requestBody, requestHeaders)

    def postDict(self, requestUrl, requestBody = None, requestHeaders={'content-type': 'application/json'}):
        """
           Accepts request body as python dictionary
        """
        requestBodyJson = None
        if requestBody: requestBodyJson = json.dumps(requestBody, indent=None, separators=(',', ':'))
        return self.request('POST', requestUrl, requestBodyJson, requestHeaders)

    def delete(self, requestUrl):
        return self.request('DELETE', requestUrl)

    def __assert_received_expected(self, received, expected, what):
        match = (received == expected)
        log = "Received {what}: {received} | Expected {what}: {expected}".format(received=received, expected=expected, what=what)
        if match: MyLogger.info(log)
        else: MyLogger.error(log)

        assert match

    def check_response_status(self, received, expected, **kwargs):
        """
        received: status code received (got from response data parsed, field 'status')
        expected: status code expected
        """
        self.__assert_received_expected(received, expected, "status code")

    #def check_expected_cause(self, response, **kwargs):
    #    """
    #    received: response data parsed where field 'body'->'cause' is analyzed
    #    kwargs: aditional regexp to match expected cause
    #    """
    #    if "expected_cause" in kwargs:
    #        received_content = response["body"]
    #        received_cause = received_content.get("cause", "")
    #        regular_expr_cause = kwargs["expected_cause"]
    #        regular_expr_flag = kwargs.get("regular_expression_flag", 0)
    #        matchObj = re.match(regular_expr_cause, received_cause, regular_expr_flag)
    #        log = 'Test error cause: "{}"~=/{}/.'.format(received_cause, regular_expr_cause)
    #        if matchObj: MyLogger.info(log)
    #        else: MyLogger.error(log)
    #
    #        assert matchObj is not None

    def check_response_body(self, received, expected, inputJsonString = False):
        """
        received: body content received (got from response data parsed, field 'body')
        expected: body content expected
        inputJsonString: input parameters as json string (default are python dictionaries)
        """
        if inputJsonString:
          # Decode json:
          received = json.loads(received)
          expected = json.loads(expected)

        self.__assert_received_expected(received, expected, "body")

    def check_response_headers(self, received, expected):
        """
        received: headers received (got from response data parsed, field 'headers')
        expected: headers expected
        """
        self.__assert_received_expected(received, expected, "headers")

    def assert_response__status_body_headers(self, response, status, bodyDict, headersDict = None):
        """
        response: Response parsed data
        status: numeric status code
        body: body dictionary to match with
        headers: headers dictionary to match with (by default, not checked: internally length and content-type application/json is verified)
        """
        self.check_response_status(response["status"], status)
        self.check_response_body(response["body"], bodyDict)
        if headersDict: self.check_response_headers(response["headers"], headersDict)


    def close(self):
      self._connection.close()


# H2AGENT Clients fixtures #####################
@pytest.fixture(scope='session')
def h2ac_admin():
  h2ac = RestClient(H2AGENT_ENDPOINT__admin)
  yield h2ac
  h2ac.close()
  print("H2AGENT-admin Teardown")

@pytest.fixture(scope='session')
def h2ac_traffic():
  h2ac = RestClient(H2AGENT_ENDPOINT__traffic)
  yield h2ac
  h2ac.close()
  print("H2AGENT-traffic Teardown")
################################################

@pytest.fixture(scope='session')
def resources():
  resourcesDict={}
  MyLogger.info("Gathering test suite resources ...")
  for resource in glob.glob('resources/*'):
    f = open(resource, "r")
    name = os.path.basename(resource)
    resourcesDict[name] = f.read()
    f.close()

  def get_resources(key, **kwargs):
    # Be careful with templates containing curly braces:
    # https://stackoverflow.com/questions/5466451/how-can-i-print-literal-curly-brace-characters-in-python-string-and-also-use-fo
    resource = resourcesDict[key]

    if kwargs:
      args = defaultdict (str, kwargs)
      resource = resource.format_map(args)

    return resource

  yield get_resources

@pytest.fixture(scope='session')
def admin_cleanup(h2ac_admin):
  def cleanup(matchingFile = None, matchingContent = { "algorithm": "FullMatching" }):
    response = h2ac_admin.delete(ADMIN_PROVISION_URI)
    response = h2ac_admin.delete(ADMIN_DATA_URI)

    if matchingFile:
      response = h2ac_admin.post(ADMIN_MATCHING_URI, resources(matchingFile))
    elif matchingContent:
      response = h2ac_admin.postDict(ADMIN_MATCHING_URI, matchingContent)

  yield cleanup

# PROVISION
VALID_PROVISION__RESPONSE_BODY = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
VALID_PROVISIONS__RESPONSE_BODY = { "result":"true", "response":"server-provision operation; valid schemas and provisions data received" }
INVALID_PROVISION_SCHEMA__RESPONSE_BODY = { "result":"false", "response":"server-provision operation; invalid schema" }
INVALID_PROVISION_DATA__RESPONSE_BODY = { "result":"false", "response":"server-provision operation; invalid provision data received" }
@pytest.fixture(scope='session')
def admin_provision(h2ac_admin, resources):
  """
  content: provide string or dictionary/list. The string will be interpreted as resources file path.
  responseBodyRef: response body reference, valid provision assumed by default.
  responseStatusRef: response status code reference, 201 by default.
  kwargs: format arguments for file content. Dictionary must be already formatted.
  """
  def send(content, responseBodyRef = VALID_PROVISION__RESPONSE_BODY, responseStatusRef = 201, **kwargs):

    request = content # assume content as dictionary
    if isinstance(content, str):
      request = resources(content)
      if kwargs: request = request.format(**kwargs)

    response = h2ac_admin.post(ADMIN_PROVISION_URI, request) if isinstance(content, str) else h2ac_admin.postDict(ADMIN_PROVISION_URI, request)
    h2ac_admin.assert_response__status_body_headers(response, responseStatusRef, responseBodyRef)

  yield send


# MATCHING
VALID_MATCHING__RESPONSE_BODY = { "result":"true", "response":"server-matching operation; valid schema and matching data received" }
INVALID_MATCHING_SCHEMA__RESPONSE_BODY = { "result":"false", "response":"server-matching operation; invalid schema" }
INVALID_MATCHING_DATA__RESPONSE_BODY = { "result":"false", "response":"server-matching operation; invalid matching data received" }
@pytest.fixture(scope='session')
def admin_matching(h2ac_admin, resources):
  """
  content: provide string or dictionary. The string will be interpreted as resources file path.
  responseBodyRef: response body reference, valid provision assumed by default.
  responseStatusRef: response status code reference, 201 by default.
  kwargs: format arguments for file content. Dictionary must be already formatted.
  """
  def send(content, responseBodyRef = VALID_MATCHING__RESPONSE_BODY, responseStatusRef = 201, **kwargs):

    request = content # assume content as dictionary
    if isinstance(content, str):
      request = resources(content)
      if kwargs: request = request.format(**kwargs)

    response = h2ac_admin.post(ADMIN_MATCHING_URI, request) if isinstance(content, str) else h2ac_admin.postDict(ADMIN_MATCHING_URI, request)
    h2ac_admin.assert_response__status_body_headers(response, responseStatusRef, responseBodyRef)

  yield send


# JSON TEMPLATES ###############################################

NESTED_NODE1_NODE2_REQUEST='''
{
  "node1": {
    "node2": "value-of-node1-node2"
  }
}
'''

BASIC_FOO_BAR_PROVISION_TEMPLATE='''
{{
  "requestMethod":"GET",
  "requestUri":"/app/v1/foo/bar/{id}",
  "responseCode":200,
  "responseBody": {{
    "foo":"bar-{id}"
  }},
  "responseHeaders": {{
    "content-type":"text/html",
    "x-version":"1.0.0"
  }}
}}
'''

# Transform better provision POST to give versatility
TRANSFORM_FOO_BAR_PROVISION_TEMPLATE='''
{{
  "requestMethod":"POST",
  "requestUri":"/app/v1/foo/bar/{id}{queryp}",
  "responseCode":200,
  "responseBody": {{
    "foo":"bar-{id}"
  }},
  "responseHeaders": {{
    "content-type":"text/html",
    "x-version":"1.0.0"
  }},
  "transform": [
    {{
      "source": "{source}",
      "target": "{target}"
    }}
  ]
}}
'''

REGEX_FOO_BAR_PROVISION_TEMPLATE='''
{{
  "requestMethod":"GET",
  "requestUri":"/app/v1/id-{id}[0-9]{{{trailing}}}/ts-[0-9]{{10}}",
  "responseCode":200,
  "responseBody": {{
    "foo":"bar-{id}"
  }},
  "responseHeaders": {{
    "content-type":"text/html",
    "x-version":"1.0.0"
  }}
}}
'''

FALLBACK_DEFAULT_PROVISION='''
{
  "requestMethod": "GET",
  "responseCode": 200,
  "responseBody": {
    "foo": "default",
    "bar": "default"
  },
  "responseHeaders": {
    "content-type": "text/html",
    "x-version": "1.0.0"
  }
}
'''

# Convert to dictionary a string with format arguments
def string2dict(content, **kwargs):
    if kwargs: content = content.format(**kwargs)
    return json.loads(content)

# Assert event which had no provision
def assertUnprovisioned(serverDataEvent, body = None, serverSequence = None):
  assert serverDataEvent["previousState"] == ""
  assert serverDataEvent["responseDelayMs"] == 0
  assert serverDataEvent["responseStatusCode"] == 501
  assert serverDataEvent["state"] == ""
  if body: assert serverDataEvent["body"] == body
  if serverSequence: assert serverDataEvent["serverSequence"] == serverSequence

  with pytest.raises(KeyError): val = serverDataEvent["virtualOriginComingFromMethod"]
  with pytest.raises(KeyError): val = serverDataEvent["responseHeaders"]
  with pytest.raises(KeyError): val = serverDataEvent["responseBody"]
  if not body:
      with pytest.raises(KeyError): val = serverDataEvent["body"]

def assertUnprovisionedServerDataItemRequestsIndex(serverDataItem, expectedMethod, expectedUri, requestsIndex, body = None):
  assert serverDataItem["method"] == expectedMethod
  assert serverDataItem["uri"] == expectedUri
  assertUnprovisioned(serverDataItem["requests"][requestsIndex], body)

