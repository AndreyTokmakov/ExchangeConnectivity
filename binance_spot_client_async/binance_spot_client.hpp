/**============================================================================
Name        : binance_spot_client.hpp
Created on  : 03.11.2026
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

#include <expected>
#include <functional>

namespace trading::binance::spot_client_asynch
{
    class BinanceSpotClient
    {
    public:
        using OrderHandler = std::function<void(OrderResponse)>;
        using CancelHandler = std::function<void(CancelResponse)>;
        using BalancesHandler = std::function<void(BalancesResponse)>;

        BinanceSpotClient(networking::RestClient& restClient,
                          ApiCredentials credentials,
                          std::string host = "testnet.binance.vision",
                          std::uint16_t port = 443,
                          std::string symbol = "BTCUSDT",
                          std::uint64_t recvWindow = 5000);

        BinanceSpotClient(const BinanceSpotClient&) = delete;
        BinanceSpotClient& operator=(const BinanceSpotClient&) = delete;

        void sendOrder(const OrderRequest& request,
                       OrderId clientOrderId,
                       OrderHandler handler);

        void cancelOrder(ExchangeOrderId exchangeOrderId,
                         CancelHandler handler);

        void getBalances(BalancesHandler handler);

    private:
        using RestResponse = networking::RestClient::Response;
        using ApiResponse = std::expected<networking::HttpResponse, BinanceApiError>;
        using ApiResponseHandler = std::function<void(ApiResponse)>;

        void asyncSignedRequest(boost::beast::http::verb method,
                                std::string_view,
                                std::string query,
                                ApiResponseHandler handler);

        networking::RestClient& restClient;
        ApiCredentials credentials;

        std::string host;
        std::uint16_t port;
        std::string symbol;
        std::uint64_t recvWindow;
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_HPP
