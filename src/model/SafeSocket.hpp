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

#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <nlohmann/json.hpp>


namespace h2agent
{
namespace model
{
class SocketManager;

/**
 * This class allows safe writting of udp sockets.
 * Write procedure could be planned with a certain delay
 */
class SafeSocket {

    std::string path_;
    int socket_;
    struct sockaddr_un server_addr_;

    boost::asio::io_context *io_context_{};

    SocketManager *socket_manager_{};

    void delayedWrite(unsigned int writeDelayUs, const std::string &data);

public:

    /**
    * Constructor
    *
    * @param socketManager parent reference to socket manager.
    * @param path socket path to write. It could be relative (to execution path) or absolute.
    * @param timersIoContext asio io context which will be used to delay write operations.
    * By default it is not used (if not provided in constructor), so delay is not performed
    * regardless the write delay configured.
    */
    SafeSocket (SocketManager *socketManager,
                const std::string& path,
                boost::asio::io_context *timersIoContext = nullptr);

    ~SafeSocket() {;}

    /**
    * Open the socket for writting
    *
    * @return Boolean about success operation.
    */
    bool open();

    /**
    * Json representation of the class instance
    */
    nlohmann::json getJson() const;

    /**
    * Write data to the socket.
    * Write could be delayed.
    *
    * @param data data to write
    * @param writeDelayUs delay for write operation. By default no delay is configured.
    * @param fromDelayed called from delayed write (used to omit instant write count). False by default.
    */
    void write(const std::string& data, unsigned int writeDelayUs = 0, bool fromDelayed = false);
};

}
}

