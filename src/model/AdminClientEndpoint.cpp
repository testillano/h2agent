/*
 ___________________________________________
|    _     ___                        _     |
|   | |   |__ \                      | |    |
|   | |__    ) |__ _  __ _  ___ _ __ | |_   |
|   | '_ \  / // _` |/ _` |/ _ \ '_ \| __|  |  HTTP/2 AGENT FOR MOCK TESTING
|   | | | |/ /| (_| | (_| |  __/ | | | |_   |  Version 0.0.z
|   |_| |_|____\__,_|\__, |\___|_| |_|\__|  |  https://github.com/testillano/h2agent
|                     __/ |                 |
|                    |___/                  |
|___________________________________________|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2021 Eduardo Ramos

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http2Client.hpp>

#include <AdminClientEndpoint.hpp>

#include <functions.hpp>


namespace h2agent
{
namespace model
{

AdminClientEndpoint::AdminClientEndpoint() {;}
AdminClientEndpoint::~AdminClientEndpoint() {;}

void AdminClientEndpoint::connect(bool fromScratch) {

    if (fromScratch) {
        client_.reset();
    }
    if (!client_) client_ = std::make_shared<ert::http2comm::Http2Client>(host_, std::to_string(port_), secure_);
}

bool AdminClientEndpoint::load(const nlohmann::json &j) {

    // Store whole document (useful for GET operation)
    json_ = j;

    // Mandatory
    auto it = j.find("id");
    key_ = *it;

    it = j.find("host");
    host_ = *it;

    it = j.find("port");
    port_ = *it;

    // Optionals
    it = j.find("secure");
    secure_ = false;
    if (it != j.end() && it->is_boolean()) {
        secure_ = *it;
    }
    it = j.find("permit");
    permit_ = true;
    if (it != j.end() && it->is_boolean()) {
        permit_ = *it;
    }

    // Validations not in schema:
    if (key_.empty()) {
        ert::tracing::Logger::error("Invalid client endpoint identifier: provide a non-empty string", ERT_FILE_LOCATION);
        return false;
    }

    if (host_.empty()) {
        ert::tracing::Logger::error("Invalid client endpoint host: provide a non-empty string", ERT_FILE_LOCATION);
        return false;
    }

    return true;
}

nlohmann::json AdminClientEndpoint::asJson() const
{
    if (!client_) return json_;

    nlohmann::json result = json_;
    result["status"] = client_->getConnectionStatus();

    return result;
}

}
}

