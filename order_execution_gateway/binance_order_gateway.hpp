/**============================================================================
Name        : binance_order_gateway.hpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_order_gateway.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_ORDER_GATEWAY_HPP
#define EXCHANGECONNECTIVITY_BINANCE_ORDER_GATEWAY_HPP

#include "order.hpp"
#include "rest_client.hpp"

#include <expected>
#include <functional>

namespace trading::binance_order_gateway
{
    struct ApiCredentials
    {
        std::string apiKey;
        std::string secretKey;
    };

    class BinanceOrderGateway
    {
    public:
        using Response = std::expected<OrderSubmission,OrderSubmissionError>;

        using ResponseHandler = std::function<void(Response)>;

        BinanceOrderGateway(networking::RestClient& restClient,
                            ApiCredentials credentials);

        BinanceOrderGateway(const BinanceOrderGateway&) = delete;
        BinanceOrderGateway& operator=(const BinanceOrderGateway&) = delete;

        void sendOrder(const OrderRequest& request,
                       OrderId clientOrderId,
                       ResponseHandler handler);

    private:
        networking::RestClient& restClient;
        ApiCredentials credentials;

        std::string host = "testnet.binance.vision";
        uint16_t port = 443;
        std::string orderEndpoint = "/api/v3/order";
        std::string symbol = "BTCUSDT";
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_ORDER_GATEWAY_HPP
