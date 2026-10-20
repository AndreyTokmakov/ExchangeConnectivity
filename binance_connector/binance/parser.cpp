/**============================================================================
Name        : parser.cpp
Created on  : 20.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : parser.cpp
============================================================================**/

#include "parser.hpp"
#include <boost/json.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace
{
    namespace json = boost::json;

    [[nodiscard]]
    const json::value& getRequired(const json::object& object, std::string_view key)
    {
        const json::value* value = object.if_contains(key);

        if (value == nullptr) {
            throw std::invalid_argument("Missing JSON field: " + std::string(key));
        }

        return *value;
    }

    [[nodiscard]]
    std::uint64_t parseUnsigned(const json::value& value)
    {
        if (value.is_uint64()) {
            return value.as_uint64();
        }

        if (value.is_int64() && value.as_int64() >= 0) {
            return static_cast<std::uint64_t>(value.as_int64());
        }

        throw std::invalid_argument("Expected a non-negative integer");
    }

    [[nodiscard]]
    std::string_view parseString(const json::value& value)
    {
        if (!value.is_string()) {
            throw std::invalid_argument("Expected a JSON string");
        }

        return value.as_string();
    }

    [[nodiscard]]
    binance::Price parseDecimal(std::string_view value)
    {
        if (value.empty()) {
            throw std::invalid_argument("Empty decimal value");
        }

        bool negative = false;

        if (value.front() == '-' || value.front() == '+') {
            negative = value.front() == '-';
            value.remove_prefix(1);
        }

        if (value.empty()) {
            throw std::invalid_argument("Invalid decimal value");
        }

        constexpr std::uint64_t DecimalScale = binance::DecimalScale;
        constexpr std::uint64_t MaxValue = static_cast<std::uint64_t>(
            std::numeric_limits<binance::Price>::max());

        const std::uint64_t limit = negative ? MaxValue + 1 : MaxValue;
        std::uint64_t integerPart = 0;
        std::uint64_t fractionalPart = 0;
        std::uint64_t fractionalScale = 1;
        bool hasIntegerDigit = false;
        bool hasFractionalDigit = false;
        bool fractionalPartStarted = false;

        for (const char character : value) {
            if (character == '.') {
                if (fractionalPartStarted) {
                    throw std::invalid_argument("Invalid decimal value");
                }

                fractionalPartStarted = true;
                continue;
            }

            if (character < '0' || character > '9') {
                throw std::invalid_argument("Invalid decimal digit");
            }

            const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');

            if (!fractionalPartStarted) {
                hasIntegerDigit = true;

                if (integerPart > (limit / DecimalScale - digit) / 10) {
                    throw std::out_of_range("Decimal value is out of range");
                }

                integerPart = integerPart * 10 + digit;
                continue;
            }

            hasFractionalDigit = true;

            if (fractionalScale < DecimalScale) {
                fractionalPart = fractionalPart * 10 + digit;
                fractionalScale *= 10;
            } else if (digit != 0) {
                throw std::out_of_range("Decimal value has excessive precision");
            }
        }

        if (!hasIntegerDigit && !hasFractionalDigit) {
            throw std::invalid_argument("Invalid decimal value");
        }

        if (fractionalPartStarted && !hasFractionalDigit) {
            throw std::invalid_argument("Invalid decimal value");
        }

        fractionalPart *= DecimalScale / fractionalScale;

        if (integerPart > limit / DecimalScale) {
            throw std::out_of_range("Decimal value is out of range");
        }

        const std::uint64_t scaledInteger = integerPart * DecimalScale;

        if (fractionalPart > limit - scaledInteger) {
            throw std::out_of_range("Decimal value is out of range");
        }

        const std::uint64_t result = scaledInteger + fractionalPart;

        if (negative) {
            if (result == MaxValue + 1) {
                return std::numeric_limits<binance::Price>::min();
            }

            return -static_cast<binance::Price>(result);
        }

        return static_cast<binance::Price>(result);
    }

    [[nodiscard]]
    binance::BookLevel parseBookLevel(const json::value& value)
    {
        if (!value.is_array() || value.as_array().size() < 2) {
            throw std::invalid_argument("Invalid order book level");
        }

        const json::array& level = value.as_array();

        return {
            parseDecimal(parseString(level[0])),
            parseDecimal(parseString(level[1]))
        };
    }

    [[nodiscard]]
    std::vector<binance::BookLevel> parseLevels(const json::value& value)
    {
        if (!value.is_array()) {
            throw std::invalid_argument("Expected an array of order book levels");
        }

        const json::array& levels = value.as_array();
        std::vector<binance::BookLevel> result;
        result.reserve(levels.size());

        for (const json::value& level : levels) {
            result.push_back(parseBookLevel(level));
        }

        return result;
    }

    [[nodiscard]]
    json::object parseObject(std::string_view message)
    {
        const json::value value = json::parse(message);

        if (!value.is_object()) {
            throw std::invalid_argument("Expected a JSON object");
        }

        return value.as_object();
    }
}

namespace binance
{
    bool Parser::isSubscriptionResponse(std::string_view message)
    {
        const json::value value = json::parse(message);

        return value.is_object() && value.as_object().if_contains("result") != nullptr;
    }

    DepthUpdate Parser::parseDepthUpdate(std::string_view message)
    {
        const json::object object = parseObject(message);

        return {
            parseUnsigned(getRequired(object, "U")),
            parseUnsigned(getRequired(object, "u")),
            parseUnsigned(getRequired(object, "E")),
            parseLevels(getRequired(object, "b")),
            parseLevels(getRequired(object, "a"))
        };
    }

    Trade Parser::parseTrade(std::string_view message)
    {
        const json::object object = parseObject(message);
        const json::value& buyerIsMaker = getRequired(object, "m");

        if (!buyerIsMaker.is_bool()) {
            throw std::invalid_argument("Expected a boolean for field: m");
        }

        return {
            parseUnsigned(getRequired(object, "t")),
            parseUnsigned(getRequired(object, "T")),
            parseDecimal(parseString(getRequired(object, "p"))),
            parseDecimal(parseString(getRequired(object, "q"))),
            buyerIsMaker.as_bool() ? Side::Sell : Side::Buy
        };
    }

    Snapshot Parser::parseSnapshot(std::string_view message)
    {
        const json::object object = parseObject(message);

        return {
            parseUnsigned(getRequired(object, "lastUpdateId")),
            parseLevels(getRequired(object, "bids")),
            parseLevels(getRequired(object, "asks"))
        };
    }
}