# Keep sorted
import base64
from collections import defaultdict
import glob

# https://stackoverflow.com/questions/72032032/importerror-cannot-import-name-iterable-from-collections-in-python
# https://github.com/testillano/h2agent/issues/xxxx Hyper import failing since alpine 3.16 (latest on May 22) because of python 3.10 packaged
# THIS IS BACKWARD COMPATIBLE (works on previous alpine 3.15)

import collections.abc
#hyper needs the four following aliases to be done manually.
collections.Iterable = collections.abc.Iterable
collections.Mapping = collections.abc.Mapping
collections.MutableSet = collections.abc.MutableSet
collections.MutableMapping = collections.abc.MutableMapping
#Now import hyper
from hyper import HTTP20Connection

import inspect
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
ADMIN_SCHEMA_URI = ADMIN_URI_PREFIX + 'schema'
ADMIN_GLOBAL_VARIABLE_URI = ADMIN_URI_PREFIX + 'global-variable'
ADMIN_SERVER_MATCHING_URI = ADMIN_URI_PREFIX + 'server-matching'
ADMIN_SERVER_PROVISION_URI = ADMIN_URI_PREFIX + 'server-provision'
ADMIN_SERVER_DATA_URI = ADMIN_URI_PREFIX + 'server-data'

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

#############
# FUNCTIONS #
#############
def currentDir(callerDistance = 2): # by default, we assume that this function will be called inside a fixture
  frame = inspect.stack()[callerDistance]
  module = inspect.getmodule(frame[0])
  return os.path.dirname(os.path.realpath(module.__file__))

######################
# CLASSES & FIXTURES #
######################

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
            content_length = "content-length"
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

# Filesystem Resources
@pytest.fixture(scope='session')
def files():
  filesDict={}
  filetypes = {'*.json'} # tuple of file types
  MyLogger.info("Preloading test suite files content into files dictionary ...")
  #os.chdir('tests')
  for filetype in filetypes:
    for _file in glob.glob("**/" + filetype, recursive = True):
      f = open(_file, "r")
      key = os.path.abspath(_file)
      filesDict[key] = f.read()
      MyLogger.debug("Files dictionary key loaded: " + key)
      f.close()
  #os.chdir('..')

  def get_files(key, callerDistance = 2, **kwargs):
    # Be careful with templates containing curly braces:
    # https://stackoverflow.com/questions/5466451/how-can-i-print-literal-curly-brace-characters-in-python-string-and-also-use-fo

    MyLogger.info("Getting files dictionary content for key {}".format(key))
    key = os.path.abspath(currentDir(callerDistance) + "/" + key)
    MyLogger.info("Absolute path key is: {}".format(key))

    _file = filesDict[key]

    if kwargs:
      args = defaultdict (str, kwargs)
      _file = _file.format_map(args)

    return _file

  yield get_files

@pytest.fixture(scope='session')
def admin_cleanup(h2ac_admin):
  def cleanup(matchingFile = None, matchingContent = { "algorithm": "FullMatching" }):
    response = h2ac_admin.delete(ADMIN_SCHEMA_URI)
    response = h2ac_admin.delete(ADMIN_GLOBAL_VARIABLE_URI)
    response = h2ac_admin.delete(ADMIN_SERVER_PROVISION_URI)
    response = h2ac_admin.delete(ADMIN_SERVER_DATA_URI)

    if matchingFile:
      response = h2ac_admin.post(ADMIN_SERVER_MATCHING_URI, files(matchingFile, callerDistance = 3))
    elif matchingContent:
      response = h2ac_admin.postDict(ADMIN_SERVER_MATCHING_URI, matchingContent)

  yield cleanup

# MATCHING
VALID_MATCHING__RESPONSE_BODY = { "result":"true", "response":"server-matching operation; valid schema and matching data received" }
INVALID_MATCHING_SCHEMA__RESPONSE_BODY = { "result":"false", "response":"server-matching operation; invalid schema" }
INVALID_MATCHING_DATA__RESPONSE_BODY = { "result":"false", "response":"server-matching operation; invalid matching data received" }
@pytest.fixture(scope='session')
def admin_server_matching(h2ac_admin, files):
  """
  content: provide string or dictionary. The string will be interpreted as resources file path.
  responseBodyRef: response body reference, valid provision assumed by default.
  responseStatusRef: response status code reference, 201 by default.
  kwargs: format arguments for file content. Dictionary must be already formatted.
  """
  def send(content, responseBodyRef = VALID_MATCHING__RESPONSE_BODY, responseStatusRef = 201, **kwargs):

    request = content # assume content as dictionary
    if isinstance(content, str):
      request = files(content, callerDistance = 3)
      if kwargs: request = request.format(**kwargs)

    response = h2ac_admin.post(ADMIN_SERVER_MATCHING_URI, request) if isinstance(content, str) else h2ac_admin.postDict(ADMIN_SERVER_MATCHING_URI, request)
    h2ac_admin.assert_response__status_body_headers(response, responseStatusRef, responseBodyRef)

  yield send

# PROVISION
VALID_PROVISION__RESPONSE_BODY = { "result":"true", "response":"server-provision operation; valid schema and provision data received" }
VALID_PROVISIONS__RESPONSE_BODY = { "result":"true", "response":"server-provision operation; valid schemas and provisions data received" }
INVALID_PROVISION_SCHEMA__RESPONSE_BODY = { "result":"false", "response":"server-provision operation; invalid schema" }
INVALID_PROVISION_DATA__RESPONSE_BODY = { "result":"false", "response":"server-provision operation; invalid provision data received" }
@pytest.fixture(scope='session')
def admin_server_provision(h2ac_admin, files):
  """
  content: provide string or dictionary/list. The string will be interpreted as resources file path.
  responseBodyRef: response body reference, valid provision assumed by default.
  responseStatusRef: response status code reference, 201 by default.
  kwargs: format arguments for file content. Dictionary must be already formatted.
  """
  def send(content, responseBodyRef = VALID_PROVISION__RESPONSE_BODY, responseStatusRef = 201, **kwargs):

    request = content # assume content as dictionary
    if isinstance(content, str):
      request = files(content, callerDistance = 3)
      if kwargs: request = request.format(**kwargs)

    response = h2ac_admin.post(ADMIN_SERVER_PROVISION_URI, request) if isinstance(content, str) else h2ac_admin.postDict(ADMIN_SERVER_PROVISION_URI, request)
    h2ac_admin.assert_response__status_body_headers(response, responseStatusRef, responseBodyRef)

  yield send

# SCHEMA
VALID_SCHEMA__RESPONSE_BODY = { "result":"true", "response":"schema operation; valid schema and schema data received" }
VALID_SCHEMAS__RESPONSE_BODY = { "result":"true", "response":"schema operation; valid schemas and schemas data received" }
INVALID_SCHEMA_SCHEMA__RESPONSE_BODY = { "result":"false", "response":"schema operation; invalid schema" }
INVALID_SCHEMA_DATA__RESPONSE_BODY = { "result":"false", "response":"schema operation; invalid schema data received" }
@pytest.fixture(scope='session')
def admin_schema(h2ac_admin, files):
  """
  content: provide string or dictionary. The string will be interpreted as resources file path.
  responseBodyRef: response body reference, valid schema assumed by default.
  responseStatusRef: response status code reference, 201 by default.
  kwargs: format arguments for file content. Dictionary must be already formatted.
  """
  def send(content, responseBodyRef = VALID_SCHEMA__RESPONSE_BODY, responseStatusRef = 201, **kwargs):

    request = content # assume content as dictionary
    if isinstance(content, str):
      request = files(content, callerDistance = 3)
      if kwargs: request = request.format(**kwargs)

    response = h2ac_admin.post(ADMIN_SCHEMA_URI, request) if isinstance(content, str) else h2ac_admin.postDict(ADMIN_SCHEMA_URI, request)
    h2ac_admin.assert_response__status_body_headers(response, responseStatusRef, responseBodyRef)

  yield send

# GLOBAL VARIABLES
VALID_GLOBAL_VARIABLES__RESPONSE_BODY = { "result":"true", "response":"global-variable operation; valid schema and global variables received" }
INVALID_GLOBAL_VARIABLES__RESPONSE_BODY = { "result":"false", "response":"global-variable operation; invalid schema" }
@pytest.fixture(scope='session')
def admin_global_variable(h2ac_admin, files):
  """
  content: provide string or dictionary. The string will be interpreted as resources file path.
  responseBodyRef: response body reference, valid global variables assumed by default.
  responseStatusRef: response status code reference, 201 by default.
  kwargs: format arguments for file content. Dictionary must be already formatted.
  """
  def send(content, responseBodyRef = VALID_GLOBAL_VARIABLES__RESPONSE_BODY, responseStatusRef = 201, **kwargs):

    request = content # assume content as dictionary
    if isinstance(content, str):
      request = files(content, callerDistance = 3)
      if kwargs: request = request.format(**kwargs)

    response = h2ac_admin.post(ADMIN_GLOBAL_VARIABLE_URI, request) if isinstance(content, str) else h2ac_admin.postDict(ADMIN_GLOBAL_VARIABLE_URI, request)
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

SCHEMAS_PROVISION_TEMPLATE='''
{{
  "requestMethod":"POST",
  "requestUri":"/app/v1/foo/bar",
  "responseCode":201,
  "responseBody": {{
    "{responseBodyField}":"test"
  }},
  "responseHeaders": {{
    "content-type":"text/html",
    "x-version":"1.0.0"
  }},
  "requestSchemaId":"{reqId}",
  "responseSchemaId":"{resId}"
}}
'''

MY_REQUESTS_SCHEMA_ID_TEMPLATE='''
{{
  "id": "{id}",
  "schema": {{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "additionalProperties": false,
    "properties": {{
      "{requiredProperty}": {{
        "type": "string"
      }}
    }},
    "required": [
      "{requiredProperty}"
    ]
  }}
}}
'''

GLOBAL_VARIABLE_1_2_3='''
{
  "var1": "value1",
  "var2": "value2",
  "var3": "value3"
}
'''

GLOBAL_VARIABLE_PROVISION_TEMPLATE_GVARCREATED_GVARREMOVED_GVARANSWERED='''
{{
  "requestMethod":"POST",
  "requestUri":"/app/v1/foo/bar",
  "responseCode":200,
  "responseBody": {{
    "foo":"bar"
  }},
  "responseHeaders": {{
    "content-type":"text/html",
    "x-version":"1.0.0"
  }},
  "transform": [
    {{
      "source": "value.{gvarcreated}value",
      "target": "globalVar.{gvarcreated}"
    }},
    {{
      "source": "eraser",
      "target": "globalVar.{gvarremoved}"
    }},
    {{
      "source": "globalVar.{gvaranswered}",
      "target": "response.body.string./gvaranswered"
    }}
  ]
}}
'''

# Transform better provision POST to give versatility
# Variables var1="var1value" and var2="var2value" are added just in case could be used by the test
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
      "source": "value.var1value",
      "target": "var.var1"
    }},
    {{
      "source": "value.var2value",
      "target": "var.var2"
    }},
    {{
      "source": "{source}",
      "target": "{target}"
    }}
  ]
}}
'''

TRANSFORM_FOO_BAR_TWO_TRANSFERS_PROVISION_TEMPLATE='''
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
      "source": "value.var1value",
      "target": "var.var1"
    }},
    {{
      "source": "value.var2value",
      "target": "var.var2"
    }},
    {{
      "source": "{source}",
      "target": "{target}"
    }},
    {{
      "source": "{source2}",
      "target": "{target2}"
    }}
  ]
}}
'''

TRANSFORM_FOO_BAR_AND_VAR1_VAR2_PROVISION_TEMPLATE='''
{{
  "requestMethod":"POST",
  "requestUri":"/app/v1/foo/bar/{id}{queryp}",
  "responseCode":200,
  "responseBody": {{
    "foo":"bar-{id}",
    "var1value": {{
      "var2value": "value-of-var1value-var2value"
    }}
  }},
  "responseHeaders": {{
    "content-type":"text/html",
    "x-version":"1.0.0"
  }},
  "transform": [
    {{
      "source": "value.var1value",
      "target": "var.var1"
    }},
    {{
      "source": "value.var2value",
      "target": "var.var2"
    }},
    {{
      "source": "{source}",
      "target": "{target}"
    }}
  ]
}}
'''

NESTED_VAR1_VAR2_REQUEST='''
{
  "var1value": {
    "var2value": "value-of-var1value-var2value"
  }
}
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
def assertUnprovisioned(serverDataEvent, requestBody = None, serverSequence = None):
  #assert serverDataEvent["previousState"] == ""
  assert serverDataEvent["responseDelayMs"] == 0
  assert serverDataEvent["responseStatusCode"] == 501
  #assert serverDataEvent["state"] == ""
  if requestBody: assert serverDataEvent["requestBody"] == requestBody
  if serverSequence: assert serverDataEvent["serverSequence"] == serverSequence

  with pytest.raises(KeyError): val = serverDataEvent["virtualOrigin"]
  with pytest.raises(KeyError): val = serverDataEvent["responseHeaders"]
  with pytest.raises(KeyError): val = serverDataEvent["responseBody"]
  if not requestBody:
      with pytest.raises(KeyError): val = serverDataEvent["requestBody"]

def assertUnprovisionedServerDataItemRequestsIndex(serverDataItem, expectedMethod, expectedUri, requestsIndex, requestBody = None):
  assert serverDataItem["method"] == expectedMethod
  assert serverDataItem["uri"] == expectedUri
  assertUnprovisioned(serverDataItem["requests"][requestsIndex], requestBody)

