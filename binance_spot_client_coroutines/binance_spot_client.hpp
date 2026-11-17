/**============================================================================
Name        : binance_spot_client.hpp
Created on  : 17.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_spot_client.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_HPP
#define EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_HPP

#include "order.hpp"
#include "rest_client.hpp"
#include "binance_api.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http/verb.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace trading::binance::spot_client
{
    class BinanceSpotClient
    {
    public:
        explicit BinanceSpotClient(boost::asio::io_context& ioContext,
                                   boost::asio::ssl::context& sslContext,
                                   ApiCredentials credentials,
                                   std::string host = "testnet.binance.vision",
                                   std::uint16_t port = 443,
                                   std::string symbol = "BTCUSDT",
                                   std::uint64_t recvWindow = 5000);

        BinanceSpotClient(const BinanceSpotClient&) = delete;
        BinanceSpotClient& operator=(const BinanceSpotClient&) = delete;

        [[nodiscard]]
        boost::asio::awaitable<OrderResponse>
        sendOrder(OrderRequest request,
                  OrderId clientOrderId);

        [[nodiscard]]
        boost::asio::awaitable<CancelResponse> cancelOrder(ExchangeOrderId exchangeOrderId);

        [[nodiscard]]
        boost::asio::awaitable<BalancesResponse> getBalances();

    private:
        using ApiResponse = std::expected<networking::HttpResponse,BinanceApiError>;

        [[nodiscard]]
        boost::asio::awaitable<ApiResponse>
        signedRequest(boost::beast::http::verb method,
                      std::string endpoint,
                      std::string query);

        networking::RestClient restClient;

        ApiCredentials credentials;

        std::string host;
        std::uint16_t port;
        std::string symbol;
        std::uint64_t recvWindow;
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_HPP
