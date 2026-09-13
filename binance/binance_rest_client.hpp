/**============================================================================
Name        : binance_rest_client.hpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_rest_client.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_REST_CLIENT_HPP
#define EXCHANGECONNECTIVITY_BINANCE_REST_CLIENT_HPP

#include "binance_types.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace exchange::binance
{
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    enum class RestError
    {
        ResolveFailed,
        ConnectFailed,
        TlsServerNameFailed,
        TlsHandshakeFailed,
        WriteFailed,
        ReadFailed,
        HttpError,
        ParseFailed
    };

    class BinanceRestClient
    {
    public:
        explicit BinanceRestClient(std::string host);

        [[nodiscard]]
        std::expected<DepthSnapshot, RestError>
        getDepthSnapshot(const std::string& symbol, std::size_t limit);

    private:
        std::string host;

        asio::io_context ioContext;
        asio::ssl::context sslContext;
        tcp::resolver resolver;
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_REST_CLIENT_HPP
