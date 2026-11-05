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

#include <expected>
#include <functional>
#include <utility>

namespace trading::binance
{
    struct ApiCredentials
    {
        std::string apiKey;
        std::string secretKey;
    };

    enum class BinanceApiErrorCode : std::uint8_t
    {
        InvalidRequest,
        SigningFailed,
        TransportError,
        ExchangeRejected,
        InvalidResponse
    };

    struct BinanceApiError
    {
        BinanceApiErrorCode code { BinanceApiErrorCode::InvalidResponse };
        std::int64_t exchangeErrorCode { 0 };
        std::string message;
    };

    struct OrderCancellation
    {
        ExchangeOrderId exchangeOrderId { 0 };
        OrderStatus status { OrderStatus::New };
    };

    struct AssetBalance
    {
        std::string asset;
        std::string free;
        std::string locked;
    };

    class BinanceSpotClient
    {
    public:
        using OrderResponse = std::expected<OrderSubmission, BinanceApiError>;
        using CancelResponse = std::expected<OrderCancellation, BinanceApiError>;
        using BalancesResponse = std::expected<std::vector<AssetBalance>, BinanceApiError>;

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
                                std::string endpoint,
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
