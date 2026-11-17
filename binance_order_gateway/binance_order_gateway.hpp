/**============================================================================
Name        : binance_order_gateway.hpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_order_gateway.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_HPP
#define EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_HPP

#include "order.hpp"
#include "rest_client.hpp"
#include "binance_api.hpp"
#include "network_runtime.hpp"

#include <expected>
#include <functional>

namespace trading::binance::spot_client_asynch
{
    struct IOrderGateway
    {
        virtual ~IOrderGateway() = default;

        virtual void send(const Order& order) = 0;
        virtual void cancel(ExchangeOrderId exchangeOrderId) = 0;
    };

    class BinanceOrderGateway
    {
    public:
        using OrderHandler  = std::function<void(OrderResponse)>;
        using CancelHandler = std::function<void(CancelResponse)>;

        BinanceOrderGateway(networking::NetworkRuntime &runtime,
                            std::string host,
                            ApiCredentials credentials,
                            std::uint16_t port = 443,
                            std::string symbol = "BTCUSDT",
                            std::uint64_t recvWindow = 5000);

        BinanceOrderGateway(const BinanceOrderGateway&) = delete;
        BinanceOrderGateway& operator=(const BinanceOrderGateway&) = delete;

        void sendOrder(const OrderRequest& request,
                       OrderId clientOrderId,
                       OrderHandler handler);

        void cancelOrder(ExchangeOrderId exchangeOrderId,
                         CancelHandler handler);


    private:
        using RestResponse = networking::RestClient::Response;
        using ApiResponse = std::expected<networking::HttpResponse, BinanceApiError>;
        using ApiResponseHandler = std::function<void(ApiResponse)>;

        void asyncSignedRequest(boost::beast::http::verb method,
                                std::string query,
                                ApiResponseHandler handler);

        networking::RestClient restClient;

        ApiCredentials credentials;
        std::string endpoint;
        std::uint16_t port;
        std::string symbol;
        std::uint64_t recvWindow;
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_SPOT_CLIENT_HPP
