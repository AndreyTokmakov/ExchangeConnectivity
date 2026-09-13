/**============================================================================
Name        : binance_types.hpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_types.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP
#define EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP

#include "market_update.hpp"

#include <string>
#include <vector>
#include <map>

namespace exchange::binance
{

    using market_data::Price;
    using market_data::Quantity;
    using market_data::SequenceNumber;
    using market_data::Timestamp;

    using OrderBookLevels = std::map<Price, Quantity>;


    struct DepthSnapshot
    {
        SequenceNumber lastUpdateId {};

        OrderBookLevels bids;
        OrderBookLevels asks;
    };

    struct DepthUpdate
    {
        SequenceNumber firstUpdateId {};
        SequenceNumber lastUpdateId {};

        Timestamp exchangeTimestamp {};

        std::vector<std::pair<Price, Quantity>> bids;
        std::vector<std::pair<Price, Quantity>> asks;
    };
}
#endif //EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP
