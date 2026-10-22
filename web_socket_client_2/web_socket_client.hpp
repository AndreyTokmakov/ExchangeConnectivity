/**============================================================================
Name        : web_socket_client.hpp
Created on  : 21.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : web_socket_client.hpp
============================================================================**/

/**
============================================================================
Name        : websocket_client.hpp
Created on  : 21.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : websocket_client.hpp
============================================================================
**/

#ifndef EXCHANGECONNECTIVITY_WEB_SOCKET_CLIENT_HPP
#define EXCHANGECONNECTIVITY_WEB_SOCKET_CLIENT_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <functional>
#include <memory>
#include <string>

namespace networking
{
    class WebSocketSession;

    class WebSocketClient
    {
    public:
        using MessageHandler = std::function<void(std::string)>;

        WebSocketClient(boost::asio::io_context& ioContext,
                        boost::asio::ssl::context& sslContext,
                        std::string host,
                        std::string port,
                        std::string target);
        ~WebSocketClient();

        WebSocketClient(const WebSocketClient&) = delete;
        WebSocketClient& operator=(const WebSocketClient&) = delete;

        void start(MessageHandler handler);
        void send(std::string message);
        void stop();

    private:
        boost::asio::io_context& ioContext;
        boost::asio::ssl::context& sslContext;
        std::string host;
        std::string port;
        std::string target;
        std::shared_ptr<WebSocketSession> session;
    };
}

#endif // EXCHANGECONNECTIVITY_WEB_SOCKET_CLIENT_HPP