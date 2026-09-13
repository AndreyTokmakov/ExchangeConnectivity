/**============================================================================
Name        : MarketData.hpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : MarketData.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP
#define EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace binance
{
    using Price = std::int64_t;
    using Quantity = std::int64_t;
    using SequenceNumber = std::uint64_t;
    using Timestamp = std::uint64_t;
    using TradeId = std::uint64_t;

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


    using OrderBookLevels = std::map<Price, Quantity>;

    struct PriceLevel
    {
        Price price {};
        Quantity quantity {};
    };

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

    struct Trade
    {
        std::string symbol;
        TradeId tradeId {};
        Price price {};
        Quantity quantity {};
        Timestamp exchangeTimestamp {};
        Timestamp tradeTimestamp {};
        bool buyerIsMaker {};
    };

    struct AggregateTrade
    {
        std::string symbol;
        TradeId aggregateTradeId {};
        TradeId firstTradeId {};
        TradeId lastTradeId {};
        Price price {};
        Quantity quantity {};
        Timestamp exchangeTimestamp {};
        Timestamp tradeTimestamp {};
        bool buyerIsMaker {};
    };

    struct BookTicker
    {
        std::string symbol;
        SequenceNumber updateId {};
        Price bidPrice {};
        Quantity bidQuantity {};
        Price askPrice {};
        Quantity askQuantity {};
    };

    struct Kline
    {
        std::string symbol;
        std::string interval;
        Timestamp openTimestamp {};
        Timestamp closeTimestamp {};
        TradeId firstTradeId {};
        TradeId lastTradeId {};
        Price openPrice {};
        Price highPrice {};
        Price lowPrice {};
        Price closePrice {};
        Quantity volume {};
        std::uint64_t tradeCount {};
        Quantity quoteAssetVolume {};
        Quantity takerBuyBaseAssetVolume {};
        Quantity takerBuyQuoteAssetVolume {};
        bool closed {};
    };

    struct Ticker
    {
        std::string symbol;
        Price priceChange {};
        double priceChangePercent {};
        Price weightedAveragePrice {};
        Price previousClosePrice {};
        Price lastPrice {};
        Quantity lastQuantity {};
        Price bidPrice {};
        Quantity bidQuantity {};
        Price askPrice {};
        Quantity askQuantity {};
        Price openPrice {};
        Price highPrice {};
        Price lowPrice {};
        Quantity volume {};
        Quantity quoteAssetVolume {};
        Timestamp openTimestamp {};
        Timestamp closeTimestamp {};
        TradeId firstTradeId {};
        TradeId lastTradeId {};
        std::uint64_t tradeCount {};
        SequenceNumber firstUpdateId {};
        SequenceNumber lastUpdateId {};
        Timestamp exchangeTimestamp {};
    };
}
#endif //EXCHANGECONNECTIVITY_BINANCE_TYPES_HPP
