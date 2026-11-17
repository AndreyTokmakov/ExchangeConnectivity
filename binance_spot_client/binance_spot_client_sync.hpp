/**============================================================================
Name        : binance_spot_client_sync.hpp
Created on  : 14.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_spot_client_sync.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_SYNC_HPP
#define EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_SYNC_HPP

#include "order.hpp"
#include "rest_client.hpp"
#include "binance_api.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <expected>
#include <string>
#include <vector>

namespace trading::binance::spot_client_synch
{
    class BinanceSpotClient
    {
    public:
        explicit BinanceSpotClient(ApiCredentials credentials,
                                   std::string host = "testnet.binance.vision",
                                   std::uint16_t port = 443,
                                   std::string symbol = "BTCUSDT",
                                   std::uint64_t recvWindow = 5000);

        BinanceSpotClient(const BinanceSpotClient&) = delete;
        BinanceSpotClient& operator=(const BinanceSpotClient&) = delete;

        [[nodiscard]]
        OrderResponse sendOrder(const OrderRequest& request, OrderId clientOrderId);

        [[nodiscard]]
        CancelResponse cancelOrder(ExchangeOrderId exchangeOrderId);

        [[nodiscard]]
        BalancesResponse getBalances();

    private:
        using RestResponse = networking::RestClient::Response;
        using ApiResponse = std::expected<networking::HttpResponse, BinanceApiError>;

        [[nodiscard]]
        ApiResponse signedRequest(boost::beast::http::verb method,
                                  std::string_view endpoint,
                                  std::string query);

        boost::asio::io_context   ioContext;
        boost::asio::ssl::context sslContext;
        networking::RestClient    restClient;

        ApiCredentials credentials;

        std::string   host;
        std::uint16_t port;
        std::string   symbol;
        std::uint64_t recvWindow;
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_SYNC_HPP
