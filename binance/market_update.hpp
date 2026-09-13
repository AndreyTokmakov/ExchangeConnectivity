//
// Created by andtokm on 9/12/26.
//

#ifndef EXCHANGECONNECTIVITY_MARKET_UPDATE_HPP
#define EXCHANGECONNECTIVITY_MARKET_UPDATE_HPP


#include <cstdint>
#include <vector>

namespace market_data
{
    using Price = std::int64_t;
    using Quantity = std::int64_t;
    using SequenceNumber = std::uint64_t;
    using Timestamp = std::uint64_t;

    struct PriceLevelUpdate
    {
        Price price {};
        Quantity quantity {};
    };

    struct MarketUpdate
    {
        SequenceNumber firstUpdateId {};
        SequenceNumber lastUpdateId {};
        Timestamp exchangeTimestamp {};

        std::vector<PriceLevelUpdate> bids;
        std::vector<PriceLevelUpdate> asks;
    };
}

#endif //EXCHANGECONNECTIVITY_MARKET_UPDATE_HPP
