/**============================================================================
Name        : types.hpp
Created on  : 20.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance types
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP
#define EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace binance
{
    using Price = std::int64_t;
    using Quantity = std::int64_t;
    using SequenceNumber = std::uint64_t;
    using Timestamp = std::uint64_t;

    inline constexpr Price DecimalScale = 100'000'000;

    enum class Side : std::uint8_t
    {
        Buy,
        Sell
    };

    enum class RestError : std::uint8_t
    {
        ResolveFailed,
        ConnectFailed,
        TlsHandshakeFailed,
        WriteFailed,
        ReadFailed,
        InvalidResponse
    };

    struct RestFailure
    {
        RestError error { RestError::InvalidResponse };
        std::string message;
    };

    struct BookLevel
    {
        Price price { 0 };
        Quantity quantity { 0 };
    };

    struct Snapshot
    {
        SequenceNumber lastUpdateId { 0 };
        std::vector<BookLevel> bids;
        std::vector<BookLevel> asks;
    };

    struct DepthUpdate
    {
        SequenceNumber firstUpdateId { 0 };
        SequenceNumber lastUpdateId { 0 };
        Timestamp exchangeTimestamp { 0 };
        std::vector<BookLevel> bids;
        std::vector<BookLevel> asks;
    };

    struct Trade
    {
        SequenceNumber sequence { 0 };
        Timestamp exchangeTimestamp { 0 };
        Price price { 0 };
        Quantity quantity { 0 };
        Side side { Side::Buy };
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP
