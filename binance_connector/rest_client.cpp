/**============================================================================
Name        : rest_client.cpp
Created on  : 20.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : rest_client.cpp
============================================================================**/

#include "rest_client.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h>

#include <memory>
#include <string>
#include <utility>

namespace
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace ssl = net::ssl;

    using Tcp = net::ip::tcp;
    using SslStream = beast::ssl_stream<beast::tcp_stream>;

    constexpr char BinanceRestHost[] = "api.binance.com";
    constexpr char HttpsPort[] = "443";

    std::string toUpper(std::string value)
    {
        for (char& character : value) {
            if (character >= 'a' && character <= 'z') {
                character = static_cast<char>(character - 'a' + 'A');
            }
        }

        return value;
    }
}

namespace binance
{
    class SnapshotRequest : public std::enable_shared_from_this<SnapshotRequest>
    {
    public:
        using Response = RestClient::Response;
        using ResponseHandler = RestClient::ResponseHandler;

        SnapshotRequest(net::io_context& ioContext,
                        ssl::context& sslContext,
                        std::string symbol,
                        ResponseHandler handler)
            : resolver(ioContext),
              stream(ioContext, sslContext),
              symbol(toUpper(std::move(symbol))),
              handler(std::move(handler))
        {
        }

        void start()
        {
            const std::shared_ptr<SnapshotRequest> self = shared_from_this();

            resolver.async_resolve(BinanceRestHost, HttpsPort,
                [self](const beast::error_code& errorCode, const Tcp::resolver::results_type& endpoints)
                {
                    if (errorCode) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::ResolveFailed, errorCode.message()
                        }));
                        return;
                    }

                    beast::get_lowest_layer(self->stream).async_connect(endpoints,
                        [self](const beast::error_code& connectError, const Tcp::endpoint&)
                        {
                            if (connectError) {
                                self->finish(std::unexpected(RestFailure {
                                    RestError::ConnectFailed, connectError.message()
                                }));
                                return;
                            }

                            self->onConnected();
                        });
                });
        }

    private:
        void onConnected()
        {
            if (SSL_set_tlsext_host_name(stream.native_handle(), BinanceRestHost) != 1) {
                finish(std::unexpected(RestFailure {
                    RestError::TlsHandshakeFailed, "Failed to configure TLS SNI"
                }));
                return;
            }

            stream.set_verify_callback(ssl::host_name_verification(BinanceRestHost));

            const std::shared_ptr<SnapshotRequest> self = shared_from_this();

            stream.async_handshake(ssl::stream_base::client,
                [self](const beast::error_code& errorCode)
                {
                    if (errorCode) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::TlsHandshakeFailed, errorCode.message()
                        }));
                        return;
                    }

                    self->sendRequest();
                });
        }

        void sendRequest()
        {
            request.version(11);
            request.method(http::verb::get);
            request.target("/api/v3/depth?symbol=" + symbol + "&limit=1000");
            request.set(http::field::host, BinanceRestHost);
            request.set(http::field::user_agent, "BinanceConnector/1.0");
            request.set(http::field::accept, "application/json");

            const std::shared_ptr<SnapshotRequest> self = shared_from_this();

            http::async_write(stream, request,
                [self](const beast::error_code& errorCode, std::size_t)
                {
                    if (errorCode) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::WriteFailed, errorCode.message()
                        }));
                        return;
                    }

                    self->readResponse();
                });
        }

        void readResponse()
        {
            const std::shared_ptr<SnapshotRequest> self = shared_from_this();

            http::async_read(stream, buffer, response,
                [self](const beast::error_code& errorCode, std::size_t)
                {
                    if (errorCode) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::ReadFailed, errorCode.message()
                        }));
                        return;
                    }

                    if (self->response.result() != http::status::ok) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::InvalidResponse,
                            "HTTP status " + std::to_string(self->response.result_int()) +
                                ": " + self->response.body()
                        }));
                        return;
                    }

                    self->finish(self->response.body());
                });
        }

        void finish(Response result)
        {
            if (completed) {
                return;
            }

            completed = true;

            if (handler) {
                handler(std::move(result));
            }
        }

        Tcp::resolver resolver;
        SslStream stream;
        beast::flat_buffer buffer;
        http::request<http::empty_body> request;
        http::response<http::string_body> response;
        std::string symbol;
        ResponseHandler handler;
        bool completed { false };
    };

    RestClient::RestClient(net::io_context& ioContext,
                                         ssl::context& sslContext)
        : ioContext(ioContext),
          sslContext(sslContext)
    {
    }

    RestClient::~RestClient() = default;

    void RestClient::asyncGetDepthSnapshot(std::string symbol, ResponseHandler handler)
    {
        const std::shared_ptr<SnapshotRequest> request = std::make_shared<SnapshotRequest>(
            ioContext, sslContext, std::move(symbol), std::move(handler));

        request->start();
    }
}
