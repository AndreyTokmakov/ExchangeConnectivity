/**============================================================================
Name        : binance_market_data_parser.hpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_market_data_parser.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_MARKET_DATA_PARSER_HPP
#define EXCHANGECONNECTIVITY_BINANCE_MARKET_DATA_PARSER_HPP

#include "binance_types.hpp"
#include "market_update.hpp"

#include <string_view>

namespace exchange::binance
{
    class BinanceMarketDataParser
    {
    public:
        [[nodiscard]]
        static  DepthUpdate parseDepthUpdate(std::string_view message) ;

        [[nodiscard]]
        static DepthSnapshot parseDepthSnapshot(std::string_view message) ;

        [[nodiscard]]
        static market_data::MarketUpdate normalize(const DepthUpdate& update) ;

        [[nodiscard]]
        static Price parsePrice(std::string_view value);

        [[nodiscard]]
        static Quantity parseQuantity(std::string_view value);
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_MARKET_DATA_PARSER_HPP
