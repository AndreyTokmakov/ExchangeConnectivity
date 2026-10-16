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

#include <format>
#include <utility>

namespace
{
    using RestError   = networking::RestError;
    using RestFailure = networking::RestFailure;
    using HttpMethod  = networking::HttpMethod;

    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace ssl = asio::ssl;
    using tcp = asio::ip::tcp;


    [[nodiscard]]
    std::unexpected<RestFailure> makeRestFailure(const RestError error,
                                                 const boost::system::error_code &systemError = {},
                                                 const unsigned long sslError = 0)
    {
        return std::unexpected(RestFailure {
            .error = error,
            .systemError = systemError,
            .sslError = sslError
        });
    }

    [[nodiscard]]
    std::optional<http::verb> toBeastVerb(const HttpMethod method)
    {
        switch (method)
        {
            case HttpMethod::Get:
                return http::verb::get;
            case HttpMethod::Post:
                return http::verb::post;
            case HttpMethod::Put:
                return http::verb::put;
            case HttpMethod::Delete:
                return http::verb::delete_;
        }

        return std::nullopt;
    }
}

namespace networking
{

    RestClient::RestClient(std::string host, std::string port):
        host { std::move(host) },
        port { std::move(port) },
        sslContext { ssl::context::tls_client },
        resolver { ioContext }
    {
        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(ssl::verify_peer);
    }

    std::expected<HttpResponse, RestFailure>
    RestClient::request(const HttpMethod method,
                        const std::string_view target,
                        const HttpHeaders& headers,
                        const std::string_view body)
    {
        const std::optional<http::verb> verb = toBeastVerb(method);
        if (!verb) {
            return makeRestFailure(RestError::InvalidMethod);
        }

        beast::error_code errorCode;
        const tcp::resolver::results_type endpoints = resolver.resolve(host, port, errorCode);
        if (errorCode) {
            return makeRestFailure(RestError::ResolveFailed, errorCode);
        }

        beast::ssl_stream<beast::tcp_stream> stream { ioContext, sslContext };
        beast::get_lowest_layer(stream).connect(endpoints, errorCode);
        if (errorCode) {
            return makeRestFailure(RestError::ConnectFailed, errorCode);
        }

        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            return makeRestFailure(RestError::TlsServerNameFailed, {}, ::ERR_get_error());
        }

        // Compatibility fallback for the current Boost.Asio build environment.
        stream.set_verify_callback(ssl::rfc2818_verification(host));

        stream.handshake(ssl::stream_base::client, errorCode);
        if (errorCode) {
            return makeRestFailure(RestError::TlsHandshakeFailed, errorCode);
        }

        http::request<http::string_body> request {
            *verb,
            beast::string_view { target.data(), target.size() },
            11
        };

        request.set(http::field::host, host);
        request.set(http::field::user_agent, UserAgent);

        for (const auto& [name, value] : headers) {
            request.set(beast::string_view { name }, beast::string_view { value });
        }

        request.body().assign(body.data(), body.size());
        request.prepare_payload();
        request.keep_alive(false);

        http::write(stream, request, errorCode);
        if (errorCode) {
            return makeRestFailure(RestError::WriteFailed, errorCode);
        }

        beast::flat_buffer buffer;
        http::response<http::string_body> response;

        http::read(stream, buffer, response, errorCode);
        if (errorCode) {
            return makeRestFailure(RestError::ReadFailed, errorCode);
        }

        HttpResponse result {
            .status = response.result_int(),
            .body = std::move(response.body())
        };

        // The complete response has been received; close the non-reusable connection.
        beast::error_code ignored;
        beast::get_lowest_layer(stream).socket().shutdown(tcp::socket::shutdown_both, ignored);
        beast::get_lowest_layer(stream).socket().close(ignored);

        return result;
    }
}