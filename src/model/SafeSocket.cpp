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
of this software and associated  documentation sockets (the "Software"), to deal
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

#include <string>
#include <memory>

#include <ert/tracing/Logger.hpp>

#include <SafeSocket.hpp>
#include <SocketManager.hpp>
#include <functions.hpp>


namespace h2agent
{
namespace model
{


SafeSocket::SafeSocket (SocketManager *socketManager, const std::string& path, boost::asio::io_context *timersIoContext):
    path_(path),
    io_context_(timersIoContext),
    socket_manager_(socketManager)
{
    open();
}

void SafeSocket::delayedWrite(unsigned int writeDelayUs, const std::string &data) {
    // metrics
    socket_manager_->incrementObservedDelayedWriteOperationCounter();

    //if (!io_context_) return; // protection
    auto timer = std::make_shared<boost::asio::steady_timer>(*io_context_, std::chrono::microseconds(writeDelayUs));
    timer->expires_after(std::chrono::microseconds(writeDelayUs));
    timer->async_wait([this, timer, data] (const boost::system::error_code& e) {
        if( e ) return; // probably, we were cancelled (boost::asio::error::operation_aborted)
        write(data, 0, true /* from delayed */);
    });
}

bool SafeSocket::open() {

    socket_ = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_ >= 0) {
        // metrics
        socket_manager_->incrementObservedOpenOperationCounter();

        memset(&server_addr_, 0, sizeof(struct sockaddr_un));
        server_addr_.sun_family = AF_UNIX;
        strcpy(server_addr_.sun_path, path_.c_str());
    }
    else {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Failed to open '%s'", path_.c_str()), ERT_FILE_LOCATION));
        // metrics
        socket_manager_->incrementObservedErrorOpenOperationCounter();
        return false;
    }

    return true;
}

nlohmann::json SafeSocket::getJson() const {
    nlohmann::json result;

    result["path"] = path_;
    result["socket"] = socket_;

    return result;
}

void SafeSocket::write (const std::string& data, unsigned int writeDelayUs, bool fromDelayed) {

    // trace data & delay
    LOGDEBUG(
        ert::tracing::Logger::debug(ert::tracing::Logger::asString("Data for write operation is: %s", data.c_str()), ERT_FILE_LOCATION);
        if (writeDelayUs != 0) ert::tracing::Logger::debug(ert::tracing::Logger::asString("Delay for write operation is: %lu", writeDelayUs), ERT_FILE_LOCATION);
    );

    // Write socket:
    //std::lock_guard<std::mutex> lock(mutex_);

    // metrics
    if (writeDelayUs == 0) {
        socket_manager_->incrementObservedWriteOperationCounter();
        if (!fromDelayed) socket_manager_->incrementObservedInstantWriteOperationCounter();
    }

    // Close socket:
    if (io_context_ && writeDelayUs != 0) {
        delayedWrite(writeDelayUs, data);
    }
    else {
        sendto(socket_, data.c_str(), data.length(), 0, (struct sockaddr*)&server_addr_, sizeof(struct sockaddr_un));
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Data written into '%s'", path_.c_str()), ERT_FILE_LOCATION));
    }
}

}
}

