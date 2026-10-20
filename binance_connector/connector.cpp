/**============================================================================
Name        : connector.cpp
Created on  : 20.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : connector.cpp
============================================================================**/

#include "connector.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <iostream>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    namespace json = boost::json;

    std::string toLower(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character){
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    std::uint64_t parseUnsigned(const json::value& value)
    {
        if (value.is_uint64()) {
            return value.as_uint64();
        }
        if (value.is_int64() && value.as_int64() >= 0) {
            return static_cast<std::uint64_t>(value.as_int64());
        }
        throw std::runtime_error("Expected a non-negative JSON integer");
    }

    std::string parseString(const json::value& value)
    {
        if (!value.is_string()) {
            throw std::runtime_error("Expected a JSON string");
        }

        return std::string(value.as_string());
    }

    binance::Price parseDecimal(std::string_view value)
    {
        if (value.empty()) {
            throw std::runtime_error("Empty decimal value");
        }

        bool negative = false;
        std::size_t position = 0;

        if (value[position] == '-' || value[position] == '+') {
            negative = value[position] == '-';
            ++position;
        }

        if (position == value.size()) {
            throw std::runtime_error("Invalid decimal value");
        }

        constexpr std::uint64_t MaxValue = std::numeric_limits<binance::Price>::max();
        const std::uint64_t limit = negative ? MaxValue + 1 : MaxValue;
        const std::uint64_t scale = binance::DecimalScale;

        std::uint64_t integerPart = 0;
        std::uint64_t fractionalPart = 0;
        std::uint64_t fractionalScale = 1;
        bool hasDigit = false;

        for (; position < value.size() && value[position] != '.'; ++position) {
            const char character = value[position];

            if (character < '0' || character > '9') {
                throw std::runtime_error("Invalid decimal digit");
            }

            hasDigit = true;
            const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');

            if (integerPart > limit / 10 ||
                (integerPart == limit / 10 && digit > limit % 10)) {
                throw std::runtime_error("Decimal value overflow");
            }

            integerPart = integerPart * 10 + digit;
        }

        if (position < value.size()) {
            ++position;

            for (; position < value.size(); ++position) {
                const char character = value[position];

                if (character < '0' || character > '9') {
                    throw std::runtime_error("Invalid fractional digit");
                }

                hasDigit = true;

                if (fractionalScale < scale) {
                    fractionalPart = fractionalPart * 10 +
                        static_cast<std::uint64_t>(character - '0');
                    fractionalScale *= 10;
                } else if (character != '0') {
                    throw std::runtime_error("Decimal precision exceeds 8 places");
                }
            }
        }

        if (!hasDigit) {
            throw std::runtime_error("Invalid decimal value");
        }

        while (fractionalScale < scale) {
            fractionalPart *= 10;
            fractionalScale *= 10;
        }

        if (integerPart > limit / scale) {
            throw std::runtime_error("Scaled decimal value overflow");
        }

        const std::uint64_t scaledInteger = integerPart * scale;

        if (fractionalPart > limit - scaledInteger) {
            throw std::runtime_error("Scaled decimal value overflow");
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

    binance::BookLevel parseBookLevel(const json::value& value)
    {
        if (!value.is_array() || value.as_array().size() < 2) {
            throw std::runtime_error("Invalid order book level");
        }

        const json::array& level = value.as_array();
        return {
            .price = parseDecimal(parseString(level[0])),
            .quantity = parseDecimal(parseString(level[1]))
        };
    }

    std::vector<binance::BookLevel> parseLevels(const json::value& value)
    {
        if (!value.is_array()) {
            throw std::runtime_error("Expected an array of order book levels");
        }

        const json::array& array = value.as_array();
        std::vector<binance::BookLevel> levels;
        levels.reserve(array.size());

        for (const json::value& level : array) {
            levels.push_back(parseBookLevel(level));
        }

        return levels;
    }

    binance::DepthUpdate parseDepthUpdate(const std::string_view message)
    {
        const json::value parsed = json::parse(message);
        const json::object& object = parsed.as_object();

        binance::DepthUpdate update;
        update.firstUpdateId = parseUnsigned(object.at("U"));
        update.lastUpdateId = parseUnsigned(object.at("u"));
        update.exchangeTimestamp = parseUnsigned(object.at("E"));
        update.bids = parseLevels(object.at("b"));
        update.asks = parseLevels(object.at("a"));

        return update;
    }

    binance::Trade parseTrade(const std::string_view message)
    {
        const json::value parsed = json::parse(message);
        const json::object& object = parsed.as_object();

        binance::Trade trade;
        trade.sequence = parseUnsigned(object.at("t"));
        trade.exchangeTimestamp = parseUnsigned(object.at("T"));
        trade.price = parseDecimal(parseString(object.at("p")));
        trade.quantity = parseDecimal(parseString(object.at("q")));

        const json::value& buyerIsMaker = object.at("m");

        if (!buyerIsMaker.is_bool()) {
            throw std::runtime_error("Invalid trade maker flag");
        }

        trade.side = buyerIsMaker.as_bool() ? binance::Side::Sell : binance::Side::Buy;
        return trade;
    }

    binance::Snapshot parseSnapshot(const std::string_view message)
    {
        const json::value parsed = json::parse(message);
        const json::object& object = parsed.as_object();

        binance::Snapshot snapshot;
        snapshot.lastUpdateId = parseUnsigned(object.at("lastUpdateId"));
        snapshot.bids = parseLevels(object.at("bids"));
        snapshot.asks = parseLevels(object.at("asks"));

        return snapshot;
    }

    bool isSubscriptionResponse(std::string_view message)
    {
        const json::value parsed = json::parse(message);

        if (!parsed.is_object()) {
            return false;
        }

        return parsed.as_object().if_contains("result") != nullptr;
    }
}

namespace binance
{
    BinanceConnector::BinanceConnector(boost::asio::io_context& ioContext, std::string symbol)
        : ioContext { ioContext },
          sslContext { boost::asio::ssl::context::tls_client },
          restClient { ioContext, sslContext },
          symbol { toLower(std::move(symbol)) }
    {
        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(boost::asio::ssl::verify_peer);
    }

    BinanceConnector::~BinanceConnector()
    {
        stop();
    }

    void BinanceConnector::startDepth(DepthHandler handler)
    {
        stopDepth();
        depthHandler = std::move(handler);

        depthClient = std::make_unique<WebSocketClient>(ioContext, sslContext, symbol + "@depth@100ms");
        depthClient->start([this](const std::string& message) {
            onDepthMessage(message);
        });
    }

    void BinanceConnector::startTrades(TradeHandler handler)
    {
        stopTrades();
        tradeHandler = std::move(handler);

        tradeClient = std::make_unique<WebSocketClient>(ioContext, sslContext, symbol + "@trade");
        tradeClient->start([this](const std::string& message){
            onTradeMessage(message);
        });
    }

    void BinanceConnector::asyncSnapshot(SnapshotHandler handler)
    {
        restClient.asyncGetDepthSnapshot(symbol,
            [this, handler = std::move(handler)](RestClient::Response response) mutable {
            onSnapshotResponse(std::move(response), handler);
        });
    }

    void BinanceConnector::stopDepth()
    {
        if (depthClient) {
            depthClient->stop();
            depthClient.reset();
        }
        depthHandler = {};
    }

    void BinanceConnector::stopTrades()
    {
        if (tradeClient) {
            tradeClient->stop();
            tradeClient.reset();
        }
        tradeHandler = {};
    }

    void BinanceConnector::stop()
    {
        stopDepth();
        stopTrades();
    }

    void BinanceConnector::onDepthMessage(const std::string& message) const
    {
        try {
            if (isSubscriptionResponse(message)) {
                return;
            }

            const DepthUpdate update = parseDepthUpdate(message);
            if (depthHandler) {
                depthHandler(update);
            }
        } catch (const std::exception& exception) {
            std::cerr << "Failed to parse depth update: " << exception.what() << '\n';
        }
    }

    void BinanceConnector::onTradeMessage(const std::string& message) const
    {
        try {
            if (isSubscriptionResponse(message)) {
                return;
            }

            const Trade trade = parseTrade(message);
            if (tradeHandler) {
                tradeHandler(trade);
            }
        } catch (const std::exception& exception) {
            std::cerr << "Failed to parse trade: " << exception.what() << '\n';
        }
    }

    void BinanceConnector::onSnapshotResponse(RestClient::Response response,
                                               const SnapshotHandler& handler) const
    {
        if (!response) {
            handler(std::unexpected(response.error()));
            return;
        }

        try {
            handler(parseSnapshot(*response));
        } catch (const std::exception& exception) {
            handler(std::unexpected(RestFailure {
                RestError::InvalidResponse, exception.what()
            }));
        }
    }
}
