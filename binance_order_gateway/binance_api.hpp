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

#include <string>
#include <expected>

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

    using OrderResponse = std::expected<OrderSubmissionResult, BinanceApiError>;
    using CancelResponse = std::expected<OrderCancellationResult, BinanceApiError>;
}

#endif //EXCHANGECONNECTIVITY_BINANCE_API_HPP
