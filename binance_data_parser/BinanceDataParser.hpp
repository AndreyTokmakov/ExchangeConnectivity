/**============================================================================
Name        : BinanceDataParser.hpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : BinanceDataParser
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_DATA_PARSER_HPP
#define EXCHANGECONNECTIVITY_BINANCE_DATA_PARSER_HPP

#include "MarketData.hpp"

#include <string_view>

namespace binance
{
    class BinanceDataParser
    {
    public:
        [[nodiscard]]
        static DepthUpdate parseDepthUpdate(std::string_view message);

        [[nodiscard]]
        static DepthSnapshot parseDepthSnapshot(std::string_view message);

        [[nodiscard]]
        static Trade parseTrade(std::string_view message);

        [[nodiscard]]
        static AggregateTrade parseAggregateTrade(std::string_view message);

        [[nodiscard]]
        static BookTicker parseBookTicker(std::string_view message);

        [[nodiscard]]
        static Kline parseKline(std::string_view message);

        [[nodiscard]]
        static Ticker parseTicker(std::string_view message);

        [[nodiscard]]
        static MarketUpdate normalize(const DepthUpdate& update);

        [[nodiscard]]
        static Price parsePrice(std::string_view value);

        [[nodiscard]]
        static Quantity parseQuantity(std::string_view value);
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_DATA_PARSER_HPP
