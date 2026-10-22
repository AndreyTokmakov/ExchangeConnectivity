/**============================================================================
Name        : web_socket_client.cpp
Created on  : 21.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : web_socket_client.cpp
============================================================================**/

#include "web_socket_client.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <openssl/ssl.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace websocket = beast::websocket;
    namespace ssl = net::ssl;

    using Tcp = net::ip::tcp;

    constexpr char BinanceHost[] = "stream.binance.com";
    constexpr char HttpsPort[] = "443";
}

namespace networking
{
    class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
    {
    public:
        using MessageHandler = WebSocketClient::MessageHandler;

        WebSocketSession(net::io_context& ioContext,
                         ssl::context& sslContext,
                         std::string streamName,
                         MessageHandler handler)
            : resolver(ioContext),
              stream(ioContext, sslContext),
              streamName(std::move(streamName)),
              handler(std::move(handler))
        {
        }

        void start()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            resolver.async_resolve(BinanceHost, HttpsPort,
                [self](const beast::error_code& errorCode, const Tcp::resolver::results_type& endpoints)
                {
                    if (errorCode) {
                        self->reportError("WebSocket resolve failed", errorCode);
                        return;
                    }

                    beast::get_lowest_layer(self->stream).async_connect(endpoints,
                        [self](const beast::error_code& connectError, const Tcp::endpoint&)
                        {
                            if (connectError) {
                                self->reportError("WebSocket connect failed", connectError);
                                return;
                            }

                            self->onConnected();
                        });
                });
        }

        void stop()
        {
            beast::error_code errorCode;
            beast::get_lowest_layer(stream).socket().cancel(errorCode);
            beast::get_lowest_layer(stream).socket().close(errorCode);
        }

    private:
        void onConnected()
        {
            if (SSL_set_tlsext_host_name(
                    stream.next_layer().native_handle(), BinanceHost) != 1) {
                std::cerr << "Failed to configure TLS SNI\n";
                stop();
                return;
            }

            stream.next_layer().set_verify_callback(ssl::host_name_verification(BinanceHost));

            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            stream.next_layer().async_handshake(ssl::stream_base::client,
                [self](const beast::error_code& errorCode)
                {
                    if (errorCode) {
                        self->reportError("WebSocket TLS handshake failed", errorCode);
                        return;
                    }

                    self->onTlsHandshake();
                });
        }

        void onTlsHandshake()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();
            stream.async_handshake(BinanceHost, "/ws",[self](const beast::error_code& errorCode){
                if (errorCode) {
                    self->reportError("WebSocket handshake failed", errorCode);
                    return;
                }
                self->subscribe();
            });
        }

        void subscribe()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            subscription = "{\"method\":\"SUBSCRIBE\",\"params\":[\"" +
                streamName + "\"],\"id\":1}";

            stream.async_write(net::buffer(subscription),
                [self](const beast::error_code& errorCode, std::size_t)
                {
                    if (errorCode) {
                        self->reportError("WebSocket subscription failed", errorCode);
                        return;
                    }

                    self->read();
                });
        }

        void read()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            stream.async_read(buffer,
                [self](const beast::error_code& errorCode, std::size_t)
                {
                    if (errorCode) {
                        if (errorCode != websocket::error::closed &&
                            errorCode != net::error::operation_aborted) {
                            self->reportError("WebSocket read failed", errorCode);
                        }

                        return;
                    }

                    std::string message = beast::buffers_to_string(self->buffer.data());
                    self->buffer.consume(self->buffer.size());

                    if (self->handler) {
                        self->handler(std::move(message));
                    }

                    self->read();
                });
        }

        void reportError(const char* operation, const beast::error_code& errorCode)
        {
            if (errorCode != net::error::operation_aborted) {
                std::cerr << operation << ": " << errorCode.message() << '\n';
            }
        }

        Tcp::resolver resolver;
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> stream;
        beast::flat_buffer buffer;
        std::string streamName;
        std::string subscription;
        MessageHandler handler;
    };

    WebSocketClient::WebSocketClient(net::io_context& ioContext,
                                                   ssl::context& sslContext,
                                                   std::string streamName)
        : ioContext(ioContext),
          sslContext(sslContext),
          streamName(std::move(streamName))
    {
    }

    WebSocketClient::~WebSocketClient()
    {
        stop();
    }

    void WebSocketClient::start(MessageHandler handler)
    {
        stop();
        session = std::make_shared<WebSocketSession>(ioContext, sslContext, streamName, std::move(handler));
        session->start();
    }

    void WebSocketClient::stop()
    {
        if (session) {
            session->stop();
            session.reset();
        }
    }
}
