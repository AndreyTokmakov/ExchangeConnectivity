/**============================================================================
Name        : binance_api.hpp
Created on  : 14.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_api.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_API_HPP
#define EXCHANGECONNECTIVITY_BINANCE_API_HPP

#include <cstdint>
#include <string>
#include <expected>

#include "types.hpp"
#include "order.hpp"

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

    using OrderResponse = std::expected<OrderSubmission, BinanceApiError>;
    using CancelResponse = std::expected<OrderCancellation, BinanceApiError>;
    using BalancesResponse = std::expected<std::vector<AssetBalance>, BinanceApiError>;
}

#endif //EXCHANGECONNECTIVITY_BINANCE_API_HPP
