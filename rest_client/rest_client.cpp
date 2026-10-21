/**============================================================================
Name        : rest_client.cpp
Created on  : 16.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : rest_client.cpp
============================================================================**/

#include "rest_client.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>


#include <openssl/err.h>
#include <openssl/ssl.h>

#include <format>
#include <utility>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace
{
    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace ssl = asio::ssl;

    using Tcp = asio::ip::tcp;
    using namespace std::string_literals;

    constexpr auto RequestTimeout = std::chrono::seconds { 10 };
    constexpr std::uint64_t MaxResponseBodySize = 16 * 1024 * 1024;

    [[nodiscard]]
    networking::RestFailure makeFailure(const networking::RestError error, std::string message)
    {
        return networking::RestFailure {
            .error = error,
            .message = std::move(message)
        };
    }
}

namespace networking
{
    class RestSession : public std::enable_shared_from_this<RestSession>
    {
    public:
        RestSession(asio::io_context& ioContext,
                    ssl::context& sslContext,
                    HttpRequest request,
                    RestClient::ResponseHandler handler)
            : resolver(ioContext),
              stream(ioContext, sslContext),
              request(std::move(request)),
              handler(std::move(handler))
        {
        }

        void run()
        {
            if (request.host.empty() || request.port.empty() || request.target.empty()) {
                complete(std::unexpected { RestFailure {
                    .error = RestError::InvalidRequest,
                    .message = "Host, port and target are required"
                }});
                return;
            }

            if (!SSL_set_tlsext_host_name(stream.native_handle(), request.host.c_str())) {
                const unsigned long error = ERR_get_error();
                const std::string message = error == 0 ? "Failed to set TLS server name"s : ERR_error_string(error, nullptr);

                complete(std::unexpected { RestFailure {
                    .error = RestError::TlsHandshakeFailed,
                    .message = message
                }});
                return;
            }

            stream.set_verify_callback(ssl::host_name_verification(request.host));

            resolver.async_resolve(request.host, request.port,
                beast::bind_front_handler(&RestSession::onResolve, shared_from_this()));
        }

    private:
        void onResolve(const beast::error_code &errorCode, const Tcp::resolver::results_type &results)
        {
            if (errorCode) {
                complete(std::unexpected { RestFailure {
                    .error = RestError::ResolveFailed,
                    .message = errorCode.message()
                }});
                return;
            }

            beast::get_lowest_layer(stream).expires_after(RequestTimeout);
            beast::get_lowest_layer(stream).async_connect(results,
                beast::bind_front_handler(&RestSession::onConnect, shared_from_this()));
        }

        void onConnect(const beast::error_code &errorCode,
                       Tcp::resolver::results_type::endpoint_type)
        {
            if (errorCode) {
                complete(std::unexpected { RestFailure {
                     .error = RestError::ConnectFailed,
                     .message = errorCode.message()
                }});
                return;
            }

            beast::get_lowest_layer(stream).expires_after(RequestTimeout);
            stream.async_handshake(ssl::stream_base::client,
                beast::bind_front_handler(&RestSession::onHandshake, shared_from_this()));
        }

        void onHandshake(const beast::error_code &errorCode)
        {
            if (errorCode) {
                complete(std::unexpected { RestFailure {
                     .error = RestError::TlsHandshakeFailed,
                     .message = errorCode.message()
                }});
                return;
            }

            http::request<http::string_body> http_request {
                request.method, request.target, 11
            };

            http_request.set(http::field::host, request.host);
            http_request.set(http::field::user_agent, "ExchangeConnectivity");
            http_request.keep_alive(false);
            http_request.body() = request.body;

            for (const auto& [name, value] : request.headers) {
                http_request.set(name, value);
            }

            if (!request.body.empty()) {
                http_request.prepare_payload();
            }

            httpRequest = std::move(http_request);

            beast::get_lowest_layer(stream).expires_after(RequestTimeout);
            http::async_write(stream, httpRequest,
                beast::bind_front_handler(&RestSession::onWrite, shared_from_this()));
        }

        void onWrite(const beast::error_code &errorCode, std::size_t)
        {
            if (errorCode) {
                complete(std::unexpected { RestFailure {
                     .error = RestError::WriteFailed,
                     .message = errorCode.message()
                }});
                return;
            }

            parser.body_limit(MaxResponseBodySize);

            beast::get_lowest_layer(stream).expires_after(RequestTimeout);
            http::async_read(stream, buffer, parser,
                beast::bind_front_handler(&RestSession::onRead, shared_from_this()));
        }

        void onRead(const beast::error_code &errorCode, std::size_t)
        {
            if (errorCode) {
                complete(std::unexpected { RestFailure {
                    .error = RestError::ReadFailed,
                    .message = errorCode.message()
               }});
                return;
            }

            http::response<http::string_body> httpResponse = parser.release();
            HttpResponse response {
                .status = httpResponse.result_int(),
                .body = std::move(httpResponse.body())
            };
            for (const auto& field : httpResponse.base()) {
                response.headers.emplace_back(
                    std::string(field.name_string().data(), field.name_string().size()),
                    std::string(field.value().data(), field.value().size()));
            }

            complete(std::move(response));
        }

        void complete(RestClient::Response&& response)
        {
            if (!handler) {
                return;
            }

            const RestClient::ResponseHandler callback = std::move(handler);
            callback(std::move(response));
        }

        Tcp::resolver resolver;
        beast::ssl_stream<beast::tcp_stream> stream;
        beast::flat_buffer buffer;
        http::response_parser<http::string_body> parser;
        HttpRequest request;
        http::request<http::string_body> httpRequest;
        RestClient::ResponseHandler handler;
    };

    RestClient::RestClient(asio::io_context& ioContext, ssl::context& sslContext)
        : ioContext(ioContext),
          sslContext(sslContext)
    {
    }

    RestClient::~RestClient() = default;

    void RestClient::asyncRequest(HttpRequest& request, ResponseHandler handler) const
    {
        std::make_shared<RestSession>(
            ioContext,
            sslContext,
            std::move(request),
            std::move(handler)
        )->run();
    }
}