#!/usr/bin/env python3
import sys
import json
import re
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit,
    QTextEdit, QPushButton, QComboBox, QTabWidget, QGridLayout, QMessageBox,
    QGroupBox, QSpacerItem, QSizePolicy, QSplitter, QStyle, QDialog
)
from PyQt5.QtCore import Qt, QUrl, QThread, pyqtSignal, QSize

# Import httpx for HTTP handling (will be used directly with the proxy you set up)
import httpx

# Class to handle HTTP requests in a separate thread
class RequestWorker(QThread):
    finished = pyqtSignal(object, object) # response (httpx.Response), error_string

    def __init__(self, full_url, method, headers, body, use_http2_prior_knowledge=False):
        super().__init__()
        self.full_url = full_url
        self.method = method
        self.headers = headers
        self.body = body # This should be b'' if no body, or bytes
        self.use_http2_prior_knowledge = use_http2_prior_knowledge # Maintained, but httpx logic is simplified

    def run(self):
        try:
            # Here, we no longer worry about httpx and http2/http1.
            # We simply assume httpx will handle the protocol according to the URL and environment.
            # The http2_prior_knowledge logic is ignored for now as requested.
            client = httpx.Client(http2=False) # Force HTTP/1.1 for initial testing if preferred, or leave as True to negotiate.
                                            # I will leave it as False to make it clear that we are not forcing HTTP/2 here.
                                            # You can change it to True if your proxy supports HTTP/2 and you want httpx to try it.

            request_kwargs = {
                "headers": self.headers,
                "timeout": 10
            }

            # We only pass 'content' if the body is NOT empty.
            # This should definitively resolve the error 'str, bytes or bytearray expected, not tuple'.
            if self.body: # If self.body is b'' (empty), this condition is False
                request_kwargs["content"] = self.body

            # Perform the request
            if self.method == "GET":
                response = client.get(self.full_url, **request_kwargs)
            elif self.method == "POST":
                response = client.post(self.full_url, **request_kwargs)
            elif self.method == "PUT":
                response = client.put(self.full_url, **request_kwargs)
            elif self.method == "DELETE":
                response = client.delete(self.full_url, **request_kwargs)
            else:
                response = client.request(self.method, self.full_url, **request_kwargs)

            self.finished.emit(response, None)
        except httpx.RequestError as exc:
            error_message = f"An error occurred while requesting {exc.request.url!r}. Details: {exc}"
            self.finished.emit(None, error_message)
        except Exception as e:
            self.finished.emit(None, str(e))


# Custom Help Dialog
class HelpDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("H2Agent Mock Tester - Help")
        self.setModal(True) # Make it a modal dialog

        layout = QVBoxLayout(self)

        self.help_text_display = QTextEdit()
        self.help_text_display.setReadOnly(True)
        # QTextEdit automatically handles HTML when setting text, so setTextFormat is not needed here.
        self.help_text_display.setText(
            """
            <h2>H2Agent Mock Tester - Help</h2>
            <p>Welcome to the H2Agent Mock Tester application. This tool allows you to send administrative, traffic, and metrics requests to h2agent mock server.</p>
            <p>The mock server should be started with proxy enabled, for example:</p>
            <p><code>H2AGENT_TRAFFIC_PROXY_PORT=8001 H2AGENT_ADMIN_PROXY_PORT=8075 ./run.sh</code></p>

            <h3>Administrative Interface Tab:</h3>
            <ul>
                <li><b>Base URL:</b> Enter the base URL for administrative requests (e.g., <code>http://localhost:8075/admin/v1</code>).</li>
                <li><b>Select Preset:</b> Choose from predefined request configurations. Selecting a preset will automatically populate the Method, URI, Headers, and Body fields.</li>
                <li><b>Method:</b> The HTTP method for the request (e.g., GET, POST).</li>
                <li><b>URI:</b> The specific URI path for the request (e.g., <code>/server-provision</code>, <code>/server-data</code>).</li>
                <li><b>Headers (JSON):</b> Enter request headers in JSON format.</li>
                <li><b>Body:</b> Enter the request body. For POST/PUT requests, this is often JSON.</li>
                <li><b>Send Request:</b> Sends the configured request to the mock server's admin endpoint.</li>
                <li><b>Clear Form:</b> Resets the form fields to their default values.</li>
            </ul>

            <h3>Traffic Interface Tab:</h3>
            <ul>
                <li>Similar to the Administrative tab, but for sending traffic-related requests (e.g., to <code>http://localhost:8001/app/v1</code>).</li>
                <li>Use this tab to simulate client requests to your application's endpoints.</li>
            </ul>

            <h3>Metrics Interface Tab:</h3>
            <ul>
                <li>This tab is for sending requests to the metrics endpoint (e.g., <code>http://localhost:8080/metrics</code>).</li>
                <li>Typically used for Prometheus-style metric scraping.</li>
                <li>The Full URL is pre-filled and read-only as metrics endpoints are usually fixed.</li>
                <li><b>Send Metrics Request:</b> Sends a GET request to the metrics URL.</li>
            </ul>

            <h3>Mock Response Section (Bottom Panel):</h3>
            <ul>
                <li><b>Status:</b> Displays the HTTP status code and message of the last response.</li>
                <li><b>Headers:</b> Shows the response headers in JSON format.</li>
                <li><b>Body:</b> Displays the response body. If it's valid JSON, it will be pretty-printed.</li>
                <li><b>Filter Body (RegEx):</b> Allows you to filter the response body using a regular expression. Only lines matching the regex will be shown.</li>
            </ul>
            """
        )
        layout.addWidget(self.help_text_display)

        # Add an OK button to close the dialog
        ok_button = QPushButton("Close")
        ok_button.clicked.connect(self.accept) # 'accept' closes the dialog with QDialog.Accepted
        layout.addWidget(ok_button, alignment=Qt.AlignRight)


class MockClientApp(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("H2Agent Mock Tester")
        self.setGeometry(400, 400, 3000, 1800) # Adjust size if necessary for larger screens

        self.last_response_body = ""
        self.request_worker = None

        self.init_ui()

    def init_ui(self):
        # We will use QSplitter to allow the user to resize the panels
        self.splitter = QSplitter(Qt.Vertical)

        # --- Request Section Container (Top part of splitter) ---
        self.request_section_widget = QWidget()
        request_section_layout = QVBoxLayout(self.request_section_widget)
        # Tabs will be inside the request section
        self.tabs = QTabWidget()

        # Layout for tabs and the info button
        tabs_and_info_layout = QHBoxLayout()
        tabs_and_info_layout.addWidget(self.tabs)

        # Info button
        self.info_button = QPushButton("Info")
        self.info_button.setIcon(self.style().standardIcon(QStyle.SP_MessageBoxInformation))
        self.info_button.clicked.connect(self.show_help_dialog)
        self.info_button.setFixedWidth(100) # Fixed width for the button
        tabs_and_info_layout.addWidget(self.info_button)
        tabs_and_info_layout.setAlignment(self.info_button, Qt.AlignRight | Qt.AlignTop) # Align button to top right

        request_section_layout.addLayout(tabs_and_info_layout)
        request_section_layout.setContentsMargins(0,0,0,0) # Remove margins to maximize space for tabs

        # --- Administrative Interface ---
        self.admin_tab = QWidget()
        self.init_admin_tab()
        self.tabs.addTab(self.admin_tab, "Administrative Interface")

        # --- Traffic Interface ---
        self.traffic_tab = QWidget()
        self.init_traffic_tab()
        self.tabs.addTab(self.traffic_tab, "Traffic Interface")

        # --- Metrics Interface ---
        self.metrics_tab = QWidget()
        self.init_metrics_tab()
        self.tabs.addTab(self.metrics_tab, "Metrics Interface")

        self.splitter.addWidget(self.request_section_widget)


        # --- Response Section (Bottom part of splitter) ---
        response_group = QGroupBox("Mock Response")
        response_layout = QVBoxLayout()

        response_layout.addWidget(QLabel("<b>Status:</b>"))
        self.response_status_label = QLabel("")
        response_layout.addWidget(self.response_status_label)

        response_layout.addWidget(QLabel("<b>Headers:</b>"))
        self.response_headers_text = QTextEdit()
        self.response_headers_text.setReadOnly(True)
        self.response_headers_text.setMinimumHeight(50) # Minimum height for headers
        # Changed to QSizePolicy.Expanding to allow manual resizing by the splitter
        self.response_headers_text.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        response_layout.addWidget(self.response_headers_text)

        response_layout.addWidget(QLabel("<b>Body:</b>"))
        self.response_body_text = QTextEdit()
        self.response_body_text.setReadOnly(True)
        self.response_body_text.setMinimumHeight(50) # Minimum height for response body
        # Ensure the body expands vertically to fill the remaining space
        self.response_body_text.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        response_layout.addWidget(self.response_body_text)

        # Regular Expression Filter for Body
        filter_layout = QHBoxLayout()
        filter_layout.addWidget(QLabel("Filter Body (RegEx):"))
        self.regex_filter_input = QLineEdit()
        self.regex_filter_input.setPlaceholderText("e.g.: ^# HELP.*$")
        self.regex_filter_input.textChanged.connect(self.apply_regex_filter)
        filter_layout.addWidget(self.regex_filter_input)

        response_layout.addLayout(filter_layout)
        response_group.setLayout(response_layout)
        self.splitter.addWidget(response_group)

        main_layout = QVBoxLayout(self) # Set the main layout to the QWidget itself
        main_layout.addWidget(self.splitter)
        self.setLayout(main_layout)

        # Set initial sizes for the splitter (optional, but useful)
        # Adjust these values to give more or less initial space to request/response.
        self.splitter.setSizes([int(self.height() * 0.4), int(self.height() * 0.6)]) # 40/60 initially

    def init_admin_tab(self):
        layout = QVBoxLayout()
        form_layout = QGridLayout()

        # Row 0-3: URL, Preset, Method, URI
        form_layout.addWidget(QLabel("Base URL:"), 0, 0)
        self.admin_base_url_input = QLineEdit("http://localhost:8075/admin/v1")
        form_layout.addWidget(self.admin_base_url_input, 0, 1)
        form_layout.addWidget(QLabel("Protocol: HTTP/2 (via Proxy)"), 0, 2)

        # Body and header examples and references:
        json__header = {"Content-Type":"application/json"}
        schema__body = {
            "id": "mySchema",
            "schema": {
                "$schema": "http://json-schema.org/draft-07/schema#",
                "type": "object",
                "additionalProperties": True,
                "properties": {
                    "foo": { "type": "number" }
                },
                "required": [ "foo" ]
            }
        }

        gvars__body = {
            "CITY": "Madrid",
            "TZ": "GMT+1"
        }

        server_provision__body = [
            {
                "requestMethod": "GET",
                "requestUri": "/app/v1/data",
                "responseCode": 200,
                "responseHeaders": {"Content-Type": "application/json"},
                "responseBody": {"foo": 1, "bar": 2}
            },
            {
                "requestMethod": "POST",
                "requestUri": "/app/v1/data",
                "responseCode": 201
            }
        ]

        #client_provision__body = [
        #]

        matching_fm__body = {
            "algorithm":"FullMatching"
        }

        matching_rm__body = {
            "algorithm":"RegexMatching"
        }

        matching_fmrr__body = {
            "algorithm":"FullMatchingRegexReplace",
            "rgx":"(555)([0-9]*)",
            "fmt":"$2"
        }

        client_endpoint__body = {
            "id": "myClientEndpoint",
            "host": "0.0.0.0",
            "port": 8181,
            "secure": False,
            "permit": False
        }

        self.admin_preset_combo = QComboBox()
        self.admin_presets = [
            {"name": "-- Select an option --", "method": "GET", "uri": "", "headers": {}, "body": ""},

            {"name": "Show Schema for schema configuration", "method": "GET", "uri": "/schema/schema", "headers": {}, "body": ""},
            {"name": "Show Schema for global variables configuration", "method": "GET", "uri": "/global-variable/schema", "headers": {}, "body": ""},
            {"name": "Show Schema for server matching (traffic classification) configuration", "method": "GET", "uri": "/server-matching/schema", "headers": {}, "body": ""},
            {"name": "Show Schema for server provision configuration", "method": "GET", "uri": "/server-provision/schema", "headers": {}, "body": ""},
            {"name": "Show Schema for client endpoints configuration", "method": "GET", "uri": "/client-endpoint/schema", "headers": {}, "body": ""},
            {"name": "Show Schema for client provision configuration", "method": "GET", "uri": "/client-provision/schema", "headers": {}, "body": ""},

            {"name": "Query Schemas", "method": "GET", "uri": "/schema", "headers": {}, "body": ""},
            {"name": "Update Schema example", "method": "POST", "uri": "/schema", "headers": json__header, "body": json.dumps(schema__body, indent=2)},
            {"name": "Delete Schemas", "method": "DELETE", "uri": "/schema", "headers": {}, "body": ""},

            {"name": "Query Global Variables", "method": "GET", "uri": "/global-variable", "headers": {}, "body": ""},
            {"name": "Query Global Variable 'CITY'", "method": "GET", "uri": "/global-variable/?name=CITY", "headers": {}, "body": ""},
            {"name": "Update Global Variable example", "method": "POST", "uri": "/global-variable", "headers": json__header, "body": json.dumps(gvars__body, indent=2)},
            {"name": "Delete Global Variables", "method": "DELETE", "uri": "/global-variable", "headers": {}, "body": ""},
            {"name": "Delete Global Variable 'CITY'", "method": "DELETE", "uri": "/global-variable?name=CITY", "headers": {}, "body": ""},

            {"name": "Query Files processed", "method": "GET", "uri": "/files", "headers": {}, "body": ""},
            {"name": "Query Files configuration", "method": "GET", "uri": "/files/configuration", "headers": {}, "body": ""},
            {"name": "Update Files configuration to enable read cache", "method": "PUT", "uri": "/files/configuration?readCache=true", "headers": {}, "body": ""},
            {"name": "Update Files configuration to disable read cache", "method": "PUT", "uri": "/files/configuration?readCache=false", "headers": {}, "body": ""},

            {"name": "Query UDP sockets processed", "method": "GET", "uri": "/udp-sockets", "headers": {}, "body": ""},

            {"name": "Query General configuration", "method": "GET", "uri": "/configuration", "headers": {}, "body": ""},

            {"name": "Query Server configuration", "method": "GET", "uri": "/server/configuration", "headers": {}, "body": ""},
            {"name": "Update Server configuration to ignore request body", "method": "PUT", "uri": "/server/configuration?receiveRequestBody=false", "headers": {}, "body": ""},
            {"name": "Update Server configuration to receive request body", "method": "PUT", "uri": "/server/configuration?receiveRequestBody=true", "headers": {}, "body": ""},
            {"name": "Update Server configuration for dynamic request body allocation", "method": "PUT", "uri": "/server/configuration?preReserveRequestBody=false", "headers": {}, "body": ""},
            {"name": "Update Server configuration for static request body allocation", "method": "PUT", "uri": "/server/configuration?preReserveRequestBody=true", "headers": {}, "body": ""},

            {"name": "Query Server Data configuration", "method": "GET", "uri": "/server-data/configuration", "headers": {}, "body": ""},
            {"name": "Update Server Data configuration to discard events", "method": "PUT", "uri": "/server-data/configuration?discard=true&discardKeyHistory=true", "headers": {}, "body": ""},
            {"name": "Update Server Data configuration to keep only the last event processed for a key", "method": "PUT", "uri": "/server-data/configuration?discard=false&discardKeyHistory=true", "headers": {}, "body": ""},
            {"name": "Update Server Data configuration to keep events", "method": "PUT", "uri": "/server-data/configuration?discard=false&discardKeyHistory=false", "headers": {}, "body": ""},
            {"name": "Update Server Data configuration to disable purge", "method": "PUT", "uri": "/server-data/configuration?disablePurge=true", "headers": {}, "body": ""},
            {"name": "Update Server Data configuration to enable purge", "method": "PUT", "uri": "/server-data/configuration?disablePurge=false", "headers": {}, "body": ""},

            {"name": "Query Client Data configuration", "method": "GET", "uri": "/client-data/configuration", "headers": {}, "body": ""},
            {"name": "Update Client Data configuration to discard events", "method": "PUT", "uri": "/client-data/configuration?discard=true&discardKeyHistory=true", "headers": {}, "body": ""},
            {"name": "Update Client Data configuration to keep only the last event processed for a key", "method": "PUT", "uri": "/client-data/configuration?discard=false&discardKeyHistory=true", "headers": {}, "body": ""},
            {"name": "Update Client Data configuration to keep events", "method": "PUT", "uri": "/client-data/configuration?discard=false&discardKeyHistory=false", "headers": {}, "body": ""},
            {"name": "Update Client Data configuration to disable purge", "method": "PUT", "uri": "/client-data/configuration?disablePurge=true", "headers": {}, "body": ""},
            {"name": "Update Client Data configuration to enable purge", "method": "PUT", "uri": "/client-data/configuration?disablePurge=false", "headers": {}, "body": ""},

            {"name": "Query Server Matching (traffic classification)", "method": "GET", "uri": "/server-matching", "headers": {}, "body": ""},
            {"name": "Update Server Matching (traffic classification) to 'full matching'", "method": "POST", "uri": "/server-matching", "headers": json__header, "body": json.dumps(matching_fm__body, indent=2)},
            {"name": "Update Server Matching (traffic classification) to 'regex matching'", "method": "POST", "uri": "/server-matching", "headers": json__header, "body": json.dumps(matching_rm__body, indent=2)},
            {"name": "Update Server Matching (traffic classification) to 'full matching regex replace'", "method": "POST", "uri": "/server-matching", "headers": json__header, "body": json.dumps(matching_fmrr__body, indent=2)},

            {"name": "Query Server Provisions", "method": "GET", "uri": "/server-provision", "headers": {}, "body": ""},
            {"name": "Update Server Provisions example", "method": "POST", "uri": "/server-provision", "headers": json__header, "body": json.dumps(server_provision__body, indent=2)},
            {"name": "Delete Server Provisions", "method": "DELETE", "uri": "/server-provision", "headers": {}, "body": ""},
            {"name": "Query Server Provisions not used", "method": "GET", "uri": "/server-provision/unused", "headers": {}, "body": ""},

            {"name": "Query Server Data", "method": "GET", "uri": "/server-data", "headers": {}, "body": ""},
            {"name": "Query Server Data summary", "method": "GET", "uri": "/server-data/summary", "headers": {}, "body": ""},
            {"name": "Query Server Data with query parameters example", "method": "GET", "uri": "/server-data?requestMethod=GET&requestUri=/app/v1/data&eventNumber=1&eventPath=/responseBody", "headers": {}, "body": ""},
            {"name": "Delete Server Data", "method": "DELETE", "uri": "/server-data", "headers": {}, "body": ""},
            {"name": "Delete Server Data with query parameters example", "method": "DELETE", "uri": "/server-data?requestMethod=GET&requestUri=/app/v1/data&eventNumber=1&eventPath=/responseBody", "headers": {}, "body": ""},

            {"name": "Query Client Data", "method": "GET", "uri": "/client-data", "headers": {}, "body": ""},
            {"name": "Query Client Data summary", "method": "GET", "uri": "/client-data/summary", "headers": {}, "body": ""},
            {"name": "Delete Client Data", "method": "DELETE", "uri": "/client-data", "headers": {}, "body": ""},

            {"name": "Query Client Endpoints", "method": "GET", "uri": "/client-endpoint", "headers": {}, "body": ""},
            {"name": "Update Client Endpoints example", "method": "POST", "uri": "/client-endpoint", "headers": {}, "body": json.dumps(client_endpoint__body, indent=2)},
            {"name": "Delete Client Endpoints", "method": "DELETE", "uri": "/client-endpoint", "headers": {}, "body": ""}

            #{"name": "Query Client Provisions", "method": "GET", "uri": "/client-provision", "headers": {}, "body": ""},
            #{"name": "Update Client Provisions example", "method": "POST", "uri": "/client-provision", "headers": json__header, "body": json.dumps(client_provision__body, indent=2)},
            #{"name": "Delete Client Provisions", "method": "DELETE", "uri": "/client-provision", "headers": {}, "body": ""},
            #{"name": "Query Client Provisions not used", "method": "GET", "uri": "/client-provision/unused", "headers": {}, "body": ""},
            #{"name": "Launch Client Provision", "method": "PUT", "uri": "/client-provision/myClientProvision", "headers": {}, "body": ""},

        ]
        for preset in self.admin_presets:
            self.admin_preset_combo.addItem(preset["name"])
        self.admin_preset_combo.currentIndexChanged.connect(self.load_admin_preset)
        form_layout.addWidget(QLabel("Select Preset:"), 1, 0)
        form_layout.addWidget(self.admin_preset_combo, 1, 1)

        self.admin_method_input = QLineEdit()
        self.admin_method_input.setReadOnly(True)
        form_layout.addWidget(QLabel("Method:"), 2, 0)
        form_layout.addWidget(self.admin_method_input, 2, 1)

        self.admin_uri_input = QLineEdit()
        form_layout.addWidget(QLabel("URI:"), 3, 0)
        form_layout.addWidget(self.admin_uri_input, 3, 1)

        # Add the form layout to the tab
        layout.addLayout(form_layout)

        # --- Headers and Body, expanding ---
        headers_body_layout = QVBoxLayout() # New layout for headers and body
        headers_body_layout.addWidget(QLabel("Headers (JSON):"))
        self.admin_headers_text = QTextEdit('{}')
        self.admin_headers_text.setMinimumHeight(50)
        self.admin_headers_text.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        headers_body_layout.addWidget(self.admin_headers_text)

        headers_body_layout.addWidget(QLabel("Body:"))
        self.admin_body_text = QTextEdit('{}')
        self.admin_body_text.setMinimumHeight(80)
        self.admin_body_text.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        headers_body_layout.addWidget(self.admin_body_text)

        # Add the headers and body layout to the tab
        layout.addLayout(headers_body_layout)

        # A stretch to push everything else up and leave space for the buttons at the bottom
        layout.addStretch(1)

        # --- Buttons at the bottom ---
        button_layout = QHBoxLayout()
        send_button = QPushButton("Send Request")
        send_button.clicked.connect(self.send_admin_request)
        button_layout.addWidget(send_button)

        clear_button = QPushButton("Clear Form")
        clear_button.clicked.connect(lambda: self.clear_form(
            self.admin_method_input, self.admin_uri_input, self.admin_headers_text, self.admin_body_text, self.admin_preset_combo,
            default_method="GET", default_headers="{}"))
        button_layout.addWidget(clear_button)

        layout.addLayout(button_layout) # Add buttons to the bottom of the tab's QVBoxLayout
        self.admin_tab.setLayout(layout)
        #self.admin_preset_combo.setCurrentIndex(1)

    def load_admin_preset(self, index):
        if 0 <= index < len(self.admin_presets):
            preset = self.admin_presets[index]
            self.admin_method_input.setText(preset["method"])
            self.admin_uri_input.setText(preset["uri"])
            self.admin_headers_text.setText(json.dumps(preset["headers"], indent=2))
            self.admin_body_text.setText(preset["body"])
        else:
            self.admin_method_input.setText("GET")
            self.admin_uri_input.setText("")
            self.admin_headers_text.setText("{}")
            self.admin_body_text.setText("")


    def init_traffic_tab(self):
        layout = QVBoxLayout()
        form_layout = QGridLayout()

        form_layout.addWidget(QLabel("Base URL:"), 0, 0)
        self.traffic_base_url_input = QLineEdit("http://localhost:8001/app/v1")
        form_layout.addWidget(self.traffic_base_url_input, 0, 1)
        form_layout.addWidget(QLabel("Protocol: HTTP/2 (via Proxy)"), 0, 2)

        # Body and header examples and references:
        json__header = {"Content-Type":"application/json"}

        self.traffic_preset_combo = QComboBox()
        self.traffic_presets = [
            {"name": "-- Select an option --", "method": "GET", "uri": "", "headers": {}, "body": ""},

            {"name": "GET example", "method": "GET", "uri": "/data", "headers": {}, "body": ""},
            {"name": "POST example", "method": "POST", "uri": "/data", "headers": json__header, "body": json.dumps({"key": "value"}, indent=2)}
        ]
        for preset in self.traffic_presets:
            self.traffic_preset_combo.addItem(preset["name"])
        self.traffic_preset_combo.currentIndexChanged.connect(self.load_traffic_preset)
        form_layout.addWidget(QLabel("Select Preset:"), 1, 0)
        form_layout.addWidget(self.traffic_preset_combo, 1, 1)

        self.traffic_method_input = QLineEdit()
        self.traffic_method_input.setReadOnly(True)
        form_layout.addWidget(QLabel("Method:"), 2, 0)
        form_layout.addWidget(self.traffic_method_input, 2, 1)

        self.traffic_uri_input = QLineEdit()
        form_layout.addWidget(QLabel("URI:"), 3, 0)
        form_layout.addWidget(self.traffic_uri_input, 3, 1)

        # Add the form layout to the tab
        layout.addLayout(form_layout)

        # --- Headers and Body, expanding ---
        headers_body_layout = QVBoxLayout() # New layout for headers and body
        headers_body_layout.addWidget(QLabel("Headers (JSON):"))
        self.traffic_headers_text = QTextEdit('{}')
        self.traffic_headers_text.setMinimumHeight(50)
        self.traffic_headers_text.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        headers_body_layout.addWidget(self.traffic_headers_text)

        headers_body_layout.addWidget(QLabel("Body:"))
        self.traffic_body_text = QTextEdit('')
        self.traffic_body_text.setMinimumHeight(80)
        self.traffic_body_text.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        headers_body_layout.addWidget(self.traffic_body_text)

        # Add the headers and body layout to the tab
        layout.addLayout(headers_body_layout)

        # A stretch to push everything else up and leave space for the buttons at the bottom
        layout.addStretch(1)

        # --- Buttons at the bottom ---
        button_layout = QHBoxLayout()
        send_button = QPushButton("Send Request")
        send_button.clicked.connect(self.send_traffic_request)
        button_layout.addWidget(send_button)

        clear_button = QPushButton("Clear Form")
        clear_button.clicked.connect(lambda: self.clear_form(
            self.traffic_method_input, self.traffic_uri_input, self.traffic_headers_text, self.traffic_body_text, self.traffic_preset_combo,
            default_method="GET", default_headers="{}"))
        button_layout.addWidget(clear_button)

        layout.addLayout(button_layout) # Add buttons to the bottom of the tab's QVBoxLayout
        self.traffic_tab.setLayout(layout)
        #self.traffic_preset_combo.setCurrentIndex(1)

    def load_traffic_preset(self, index):
        if 0 <= index < len(self.traffic_presets):
            preset = self.traffic_presets[index]
            self.traffic_method_input.setText(preset["method"])
            self.traffic_uri_input.setText(preset["uri"])
            self.traffic_headers_text.setText(json.dumps(preset["headers"], indent=2))
            self.traffic_body_text.setText(preset["body"])
        else:
            self.traffic_method_input.setText("GET")
            self.traffic_uri_input.setText("")
            self.traffic_headers_text.setText("{}")
            self.traffic_body_text.setText("")

    def init_metrics_tab(self):
        # This tab is much simpler
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Full URL:"))
        self.metrics_full_url_input = QLineEdit("http://localhost:8080/metrics")
        self.metrics_full_url_input.setReadOnly(True)
        self.metrics_full_url_input.setStyleSheet("background-color: #e0e0e0;")
        self.metrics_full_url_input.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        layout.addWidget(self.metrics_full_url_input)

        layout.addWidget(QLabel("Protocol: HTTP/1.1 (via Proxy)"))

        button_layout = QHBoxLayout()
        send_button = QPushButton("Send Metrics Request")
        send_button.clicked.connect(self.send_metrics_request)
        button_layout.addWidget(send_button)
        layout.addLayout(button_layout)

        layout.addStretch(1) # This stretch pushes elements up
        self.metrics_tab.setLayout(layout)

    def show_help_dialog(self):
        help_dialog = HelpDialog(self)
        # Set the size of the help dialog based on the main window's size
        main_window_size = self.size()
        # You can adjust the multiplier to make it slightly smaller than the main window
        help_dialog.resize(int(main_window_size.width() * 0.8), int(main_window_size.height() * 0.8))
        help_dialog.exec_() # Use exec_() to show it as a modal dialog

    def clear_form(self, method_input, uri_input, headers_text, body_text, preset_combo=None, default_method="", default_headers="{}"):
        # Ensure QTextEdit widgets are cleared correctly
        headers_text.setText(default_headers)
        body_text.setText("")

        # For QLineEdit, use setText
        if not method_input.isReadOnly():
            method_input.setText(default_method)
        if not uri_input.isReadOnly():
            uri_input.setText("")

        if preset_combo:
            preset_combo.setCurrentIndex(0)


    def clear_response_ui(self):
        self.response_status_label.setText("")
        self.response_headers_text.setText("")
        self.response_body_text.setText("")
        self.regex_filter_input.clear()
        self.last_response_body = ""

    def send_request(self, base_url_or_full_url_input, method_input_or_none, uri_input_or_none, headers_text_or_none, body_text_or_none, use_http2_prior_knowledge=False):
        self.clear_response_ui()

        if uri_input_or_none is None:
            full_url = base_url_or_full_url_input.text().strip()
        else:
            base_url = base_url_or_full_url_input.text().strip()
            uri = uri_input_or_none.text().strip()
            full_url = f"{base_url}{uri}"

        if not full_url:
            QMessageBox.warning(self, "Input Error", "Please enter the Base or Full URL.")
            return

        method = "GET"
        if method_input_or_none is not None:
            method = method_input_or_none.text().strip().upper()

        headers = {}
        headers_raw = ""
        if headers_text_or_none is not None:
            headers_raw = headers_text_or_none.toPlainText()

        if headers_raw:
            try:
                headers = json.loads(headers_raw)
            except json.JSONDecodeError as e:
                QMessageBox.warning(self, "Headers Error", f"Error parsing JSON headers: {e}")
                return

        body_bytes = b''
        if body_text_or_none is not None:
            body_raw = body_text_or_none.toPlainText()
            body_bytes = body_raw.encode('utf-8') if body_raw else b''

        if self.request_worker and self.request_worker.isRunning():
            QMessageBox.warning(self, "Request in Progress", "A request is already in progress. Please wait or cancel it.")
            return

        self.response_status_label.setText("Sending request...")
        self.response_status_label.setStyleSheet("color: blue;")

        self.request_worker = RequestWorker(full_url, method, headers, body_bytes, use_http2_prior_knowledge)
        self.request_worker.finished.connect(self.handle_httpx_response)
        self.request_worker.start()


    def send_admin_request(self):
        self.send_request(
            self.admin_base_url_input,
            self.admin_method_input,
            self.admin_uri_input,
            self.admin_headers_text,
            self.admin_body_text,
            use_http2_prior_knowledge=True
        )

    def send_traffic_request(self):
        self.send_request(
            self.traffic_base_url_input,
            self.traffic_method_input,
            self.traffic_uri_input,
            self.traffic_headers_text,
            self.traffic_body_text,
            use_http2_prior_knowledge=True
        )

    def send_metrics_request(self):
        self.send_request(
            self.metrics_full_url_input,
            None,
            None,
            None,
            None,
            use_http2_prior_knowledge=False
        )

    def handle_httpx_response(self, response, error_string):
        if error_string:
            self.response_status_label.setText(f"Error: {error_string}")
            self.response_status_label.setStyleSheet("color: red;")
            self.response_body_text.setText(f"Error detail: {error_string}")
            self.last_response_body = ""
        elif response:
            status_code = response.status_code
            status_text = response.reason_phrase
            self.response_status_label.setText(f"{status_code} {status_text}")
            self.response_status_label.setStyleSheet("color: green;")

            headers_dict = dict(response.headers)
            self.response_headers_text.setText(json.dumps(headers_dict, indent=2))

            body = response.text
            self.last_response_body = body

            try:
                json_body = json.loads(body)
                self.response_body_text.setText(json.dumps(json_body, indent=2))
            except json.JSONDecodeError:
                self.response_body_text.setText(body)

            self.apply_regex_filter()
        else:
            self.response_status_label.setText("No response received.")
            self.response_status_label.setStyleSheet("color: orange;")
            self.last_response_body = ""

        self.request_worker = None


    def apply_regex_filter(self):
        regex_pattern = self.regex_filter_input.text().strip()

        if not self.last_response_body:
            self.response_body_text.setText("")
            return

        if not regex_pattern:
            try:
                json_body = json.loads(self.last_response_body)
                self.response_body_text.setText(json.dumps(json_body, indent=2))
            except json.JSONDecodeError:
                self.response_body_text.setText(self.last_response_body)
            self.response_body_text.setStyleSheet("")
            return

        filtered_lines = []
        try:
            regex = re.compile(regex_pattern)
            for line in self.last_response_body.splitlines():
                if regex.search(line):
                    filtered_lines.append(line)
            self.response_body_text.setText("\n".join(filtered_lines))
            self.response_body_text.setStyleSheet("")
        except re.error as e:
            self.response_body_text.setText(f"Regular expression error: {e}\n\n{self.last_response_body}")
            self.response_body_text.setStyleSheet("color: red;")


if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = MockClientApp()
    ex.show()
    sys.exit(app.exec_())

