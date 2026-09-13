/**============================================================================
Name        : binance_market_data_parser.cpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_market_data_parser.cpp
============================================================================**/

#include "binance_market_data_parser.hpp"

#include <simdjson.h>

namespace
{
    constexpr std::int64_t DecimalScale = 100'000'000;

    std::int64_t parseDecimal(std::string_view value)
    {
        bool negative = false;
        std::size_t index = 0;

        if (!value.empty() && value.front() == '-')
        {
            negative = true;
            index = 1;
        }

        std::int64_t integerPart = 0;

        while (index < value.size() && value[index] != '.')
        {
            integerPart =
                integerPart * 10 +
                static_cast<std::int64_t>(value[index] - '0');

            ++index;
        }

        std::int64_t fractionalPart = 0;
        std::size_t fractionalDigits = 0;

        if (index < value.size() && value[index] == '.')
        {
            ++index;
            while (index < value.size() && fractionalDigits < 8)
            {
                fractionalPart = fractionalPart * 10 + static_cast<std::int64_t>(value[index] - '0');
                ++fractionalDigits;
                ++index;
            }
        }

        while (fractionalDigits < 8)
        {
            fractionalPart *= 10;
            ++fractionalDigits;
        }

        const std::int64_t result = integerPart * DecimalScale + fractionalPart;

        return negative ? -result : result;
    }
}

namespace exchange::binance
{

    DepthUpdate BinanceMarketDataParser::parseDepthUpdate(const std::string_view message)
    {
        simdjson::dom::parser parser;
        const simdjson::dom::element document = parser.parse(message.data(), message.size());

        DepthUpdate update;

        update.exchangeTimestamp = static_cast<Timestamp>(document["E"].get_uint64());
        update.firstUpdateId     = static_cast<SequenceNumber>(document["U"].get_uint64());
        update.lastUpdateId      = static_cast<SequenceNumber>(document["u"].get_uint64());

        for (const auto& bids = document["b"].get_array(); const auto& level : bids)
        {
            const simdjson::dom::array values =level.get_array();
            const std::string_view price =values.at(0).get_string();
            const std::string_view quantity =values.at(1).get_string();
            update.bids.emplace_back(parsePrice(price),parseQuantity(quantity));
        }

        for (const auto& asks = document["a"].get_array(); const auto& level : asks)
        {
            const simdjson::dom::array values = level.get_array();
            const std::string_view price = values.at(0).get_string();
            const std::string_view quantity = values.at(1).get_string();
            update.asks.emplace_back(parsePrice(price),parseQuantity(quantity));
        }

        return update;
    }

    DepthSnapshot BinanceMarketDataParser::parseDepthSnapshot(const std::string_view message)
    {
        simdjson::dom::parser parser;
        const simdjson::dom::element document = parser.parse(message.data(), message.size());

        DepthSnapshot snapshot;

        snapshot.lastUpdateId = static_cast<SequenceNumber>(document["lastUpdateId"].get_uint64());
        for (const auto& bids = document["bids"].get_array(); const auto& level : bids)
        {
            const simdjson::dom::array values = level.get_array();
            const std::string_view price = values.at(0).get_string();
            const std::string_view quantity = values.at(1).get_string();
            snapshot.bids.emplace(parsePrice(price),parseQuantity(quantity));
        }

        for (const auto& asks = document["asks"].get_array(); const auto& level : asks)
        {
            const simdjson::dom::array values = level.get_array();
            const std::string_view price = values.at(0).get_string();
            const std::string_view quantity = values.at(1).get_string();
            snapshot.asks.emplace(parsePrice(price), parseQuantity(quantity));
        }

        return snapshot;
    }

    market_data::MarketUpdate
    BinanceMarketDataParser::normalize(const DepthUpdate& update)
    {
        market_data::MarketUpdate result;

        result.firstUpdateId = update.firstUpdateId;
        result.lastUpdateId = update.lastUpdateId;
        result.exchangeTimestamp = update.exchangeTimestamp;
        result.bids.reserve(update.bids.size());
        result.asks.reserve(update.asks.size());

        for (const auto& [price, quantity] : update.bids){
            result.bids.emplace_back(price, quantity);
        }
        for (const auto& [price, quantity] : update.asks) {
            result.asks.emplace_back(price, quantity);
        }
        return result;
    }

    Price BinanceMarketDataParser::parsePrice(const std::string_view value) {
        return parseDecimal(value);
    }

    Quantity BinanceMarketDataParser::parseQuantity(const std::string_view value) {
        return parseDecimal(value);
    }
}