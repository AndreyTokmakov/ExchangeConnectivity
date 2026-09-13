/**============================================================================
Name        : binance_rest_client.cpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_rest_client.cpp
============================================================================**/

#include "binance_rest_client.hpp"
#include "binance_market_data_parser.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <format>
#include <utility>

namespace exchange::binance
{
    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace ssl   = asio::ssl;

    using tcp = asio::ip::tcp;

    BinanceRestClient::BinanceRestClient(std::string host):
        host { std::move(host) },
        sslContext { ssl::context::tls_client },
        resolver { ioContext}
    {
        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(ssl::verify_peer);
    }

    std::expected<DepthSnapshot, RestError>
    BinanceRestClient::getDepthSnapshot(const std::string& symbol,
                                        std::size_t limit)
    {
        beast::error_code errorCode;
        const tcp::resolver::results_type endpoints = resolver.resolve(host, "443", errorCode);
        if (errorCode) {
            return std::unexpected(RestError::ResolveFailed);
        }

        beast::ssl_stream<beast::tcp_stream> stream{ioContext, sslContext};
        asio::connect(stream.next_layer().socket(), endpoints.begin(), endpoints.end(), errorCode);
        if (errorCode) {
            return std::unexpected(RestError::ConnectFailed);
        }

        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            return std::unexpected(RestError::TlsServerNameFailed);
        }

        stream.handshake(ssl::stream_base::client, errorCode);
        if (errorCode) {
            return std::unexpected(RestError::TlsHandshakeFailed);
        }

        const std::string target = std::format("/api/v3/depth?symbol={}&limit={}", symbol, limit);
        http::request<http::string_body> request {http::verb::get, target, 11 };

        request.set(http::field::host, host);
        request.set(http::field::user_agent, "hft-binance-connector/1.0");

        http::write(stream, request);

        beast::flat_buffer buffer;
        http::response<http::string_body> response;

        http::read(stream, buffer, response, errorCode);
        if (errorCode) {
            return std::unexpected(RestError::ReadFailed);
        }
        if (response.result() != http::status::ok) {
            return std::unexpected(RestError::HttpError);
        }

        beast::error_code shutdownError;
        stream.shutdown(shutdownError);

        return BinanceMarketDataParser::parseDepthSnapshot(response.body());
    }
}