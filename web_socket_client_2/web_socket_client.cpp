/**============================================================================
Name        : web_socket_client.cpp
Created on  : 21.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : web_socket_client.cpp
============================================================================**/

#include "web_socket_client.hpp"


#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <openssl/ssl.h>

#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace websocket = beast::websocket;
    namespace ssl = net::ssl;

    using Tcp = net::ip::tcp;
}

namespace networking
{
    class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
    {
    public:
        using MessageHandler = WebSocketClient::MessageHandler;

        WebSocketSession(net::io_context& ioContext,
                         ssl::context& sslContext,
                         std::string host,
                         std::string port,
                         std::string target,
                         MessageHandler handler)
            : strand { net::make_strand(ioContext) },
              resolver { ioContext },
              stream { ioContext, sslContext },
              host { std::move(host) },
              port { std::move(port) },
              target { std::move(target) },
              handler { std::move(handler) }
        {
        }

        void start()
        {
            net::post(strand, [self = shared_from_this()] {
                if (State::Idle != self->state) {
                    return;
                }
                self->state = State::Connecting;
                self->resolve();
            });
        }

        void send(std::string message)
        {
            net::post(strand, [self = shared_from_this(), message = std::move(message)]() mutable {
                if (State::Stopped == self->state) {
                    return;
                }
                self->outgoingMessages.push_back(std::move(message));
                self->write();
            });
        }

        void stop()
        {
            net::post(strand, [self = shared_from_this()] {
                self->stopNow();
            });
        }

    private:
        enum class State : std::uint8_t
        {
            Idle,
            Connecting,
            Connected,
            Stopped
        };

        void resolve()
        {
            resolver.async_resolve(host, port, net::bind_executor(strand,
                beast::bind_front_handler(&WebSocketSession::onResolve, shared_from_this())
            ));
        }

        void onResolve(const beast::error_code& errorCode,
                       const Tcp::resolver::results_type& endpoints)
        {
            if (State::Connecting != state) {
                return;
            }
            if (errorCode) {
                fail("WebSocket resolve failed", errorCode);
                return;
            }

            beast::get_lowest_layer(stream).async_connect(endpoints, net::bind_executor(strand,
                beast::bind_front_handler(&WebSocketSession::onConnect, shared_from_this())
            ));
        }

        void onConnect(const beast::error_code& errorCode, const Tcp::endpoint&)
        {
            if (State::Connecting != state) {
                return;
            }
            if (errorCode) {
                return fail("WebSocket connect failed", errorCode);
            }
            if (SSL_set_tlsext_host_name(stream.next_layer().native_handle(), host.c_str()) != 1) {
                std::cerr << "Failed to configure TLS SNI for " << host << '\n';
                stopNow();
                return;
            }

            stream.next_layer().set_verify_callback(ssl::host_name_verification(host));
            stream.next_layer().async_handshake(ssl::stream_base::client, net::bind_executor(strand,
                beast::bind_front_handler(&WebSocketSession::onTlsHandshake, shared_from_this())
            ));
        }

        void onTlsHandshake(const beast::error_code& errorCode)
        {
            if (State::Connecting != state) {
                return;
            }
            if (errorCode) {
                return fail("WebSocket TLS handshake failed", errorCode);
            }

            stream.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
            stream.async_handshake(host, target, net::bind_executor(strand,
                beast::bind_front_handler(&WebSocketSession::onHandshake, shared_from_this())
            ));
        }

        void onHandshake(const beast::error_code& errorCode)
        {
            if (State::Connecting != state) {
                return;
            }
            if (errorCode) {
                return fail("WebSocket handshake failed", errorCode);
            }

            state = State::Connected;
            read();
            write();
        }

        void read()
        {
            if (State::Connected != state) {
                return;
            }
            stream.async_read(buffer, net::bind_executor(strand,
                beast::bind_front_handler(&WebSocketSession::onRead, shared_from_this())
            ));
        }

        void onRead(const beast::error_code& errorCode, std::size_t)
        {
            if (State::Connected != state) {
                return;
            }

            if (errorCode) {
                if (errorCode != websocket::error::closed) {
                    fail("WebSocket read failed", errorCode);
                }
                else {
                    stopNow();
                }
                return;
            }

            std::string message = beast::buffers_to_string(buffer.data());
            buffer.consume(buffer.size());

            if (handler) {
                handler(std::move(message));
            }

            read();
        }

        void write()
        {
            if (State::Connected != state || writing || outgoingMessages.empty()) {
                return;
            }

            writing = true;
            stream.async_write(net::buffer(outgoingMessages.front()), net::bind_executor(strand,
                beast::bind_front_handler(&WebSocketSession::onWrite, shared_from_this())
            ));
        }

        void onWrite(const beast::error_code& errorCode, std::size_t)
        {
            if (State::Stopped == state) {
                return;
            }
            writing = false;
            if (errorCode) {
                fail("WebSocket write failed", errorCode);
                return;
            }

            outgoingMessages.pop_front();
            write();
        }

        void fail(const std::string_view operation, const beast::error_code& errorCode)
        {
            if (errorCode != net::error::operation_aborted) {
                std::cerr << operation << ": " << errorCode.message() << '\n';
            }
            stopNow();
        }

        void stopNow()
        {
            if (State::Stopped == state) {
                return;
            }

            state = State::Stopped;
            writing = false;
            outgoingMessages.clear();
            resolver.cancel();

            beast::error_code error;
            const auto _ = beast::get_lowest_layer(stream).socket().close(error);
        }

        net::strand<net::io_context::executor_type> strand;
        Tcp::resolver resolver;
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> stream;
        beast::flat_buffer buffer;
        std::string host;
        std::string port;
        std::string target;
        MessageHandler handler;
        std::deque<std::string> outgoingMessages;
        State state { State::Idle };
        bool writing { false };
    };

    WebSocketClient::WebSocketClient(net::io_context& ioContext,
                                     ssl::context& sslContext,
                                     std::string host,
                                     std::string port,
                                     std::string target)
        : ioContext(ioContext),
          sslContext(sslContext),
          host(std::move(host)),
          port(std::move(port)),
          target(std::move(target))
    {
    }

    WebSocketClient::~WebSocketClient()
    {
        stop();
    }

    void WebSocketClient::start(MessageHandler handler)
    {
        stop();

        session = std::make_shared<WebSocketSession>(
            ioContext,
            sslContext,
            host,
            port,
            target,
            std::move(handler)
        );

        session->start();
    }

    void WebSocketClient::send(std::string message) const
    {
        if (session) {
            session->send(std::move(message));
        }
    }

    void WebSocketClient::stop()
    {
        if (session) {
            session->stop();
            session.reset();
        }
    }
}