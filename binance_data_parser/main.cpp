/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/

#include <format>
#include <string_view>
#include <vector>
#include <filesystem>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "BinanceDataParser.hpp"

namespace
{
    constexpr std::filesystem::path dataDir() noexcept
    {
        return std::filesystem::current_path() / "../../binance_data_parser/data/";
    }

    std::string readFile(const std::string& path)
    {
        std::ifstream file(path);

        if (!file)
        {
            throw std::runtime_error{"Failed to open test data: " + path};
        }

        return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }

    void check(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error{"Test failed: " + message};
        }
    }

    template<typename T, typename U>
    void checkEqual(const T& actual, const U& expected, const std::string& field)
    {
        if (actual != expected)
        {
            throw std::runtime_error{
                "Test failed: " + field
            };
        }
    }
}

namespace tests
{
    void testParseTrade()
    {
        const std::string message = readFile(dataDir () / "trade.json");
        const binance::Trade trade = binance::BinanceDataParser::parseTrade(message);

        checkEqual(trade.symbol, "BNBBTC", "trade.symbol");
        checkEqual(trade.tradeId, 12345ULL, "trade.tradeId");
        checkEqual(trade.price, 100'000, "trade.price");
        checkEqual(trade.quantity, 10'000'000'000LL, "trade.quantity");
        checkEqual(trade.exchangeTimestamp, 1672515782136ULL, "trade.exchangeTimestamp");
        checkEqual(trade.tradeTimestamp, 1672515782136ULL, "trade.tradeTimestamp");
        check(trade.buyerIsMaker, "trade.buyerIsMaker");
    }

    void testParseAggregateTrade()
    {
        const std::string message = readFile(dataDir () / "aggregate_trade.json");
        const binance::AggregateTrade trade = binance::BinanceDataParser::parseAggregateTrade(message);

        checkEqual(trade.symbol, "BNBBTC", "aggregateTrade.symbol");
        checkEqual(trade.aggregateTradeId, 5933014ULL, "aggregateTrade.aggregateTradeId");
        checkEqual(trade.firstTradeId, 100ULL, "aggregateTrade.firstTradeId");
        checkEqual(trade.lastTradeId, 105ULL, "aggregateTrade.lastTradeId");
        checkEqual(trade.price, 100'000, "aggregateTrade.price");
        checkEqual(trade.quantity, 10'000'000'000LL, "aggregateTrade.quantity");
        checkEqual(trade.exchangeTimestamp, 1672515782136ULL, "aggregateTrade.exchangeTimestamp");
        checkEqual(trade.tradeTimestamp, 1672515782136ULL, "aggregateTrade.tradeTimestamp");
        check(trade.buyerIsMaker, "aggregateTrade.buyerIsMaker");
    }

    void testParseBookTicker()
    {
        const std::string message = readFile(dataDir () / "book_ticker.json");
        const binance::BookTicker ticker = binance::BinanceDataParser::parseBookTicker(message);

        checkEqual(ticker.symbol, "BNBUSDT", "bookTicker.symbol");
        checkEqual(ticker.updateId, 400900217ULL, "bookTicker.updateId");
        checkEqual(ticker.bidPrice, 2'535'190'000LL, "bookTicker.bidPrice");
        checkEqual(ticker.bidQuantity, 3'121'000'000LL, "bookTicker.bidQuantity");
        checkEqual(ticker.askPrice, 2'536'520'000LL, "bookTicker.askPrice");
        checkEqual(ticker.askQuantity, 4'066'000'000LL, "bookTicker.askQuantity");
    }

    void testParseKline()
    {
        const std::string message = readFile(dataDir () / "kline.json");
        const binance::Kline kline = binance::BinanceDataParser::parseKline(message);

        checkEqual(kline.symbol, "BNBBTC", "kline.symbol");
        checkEqual(kline.interval, "1m", "kline.interval");
        checkEqual(kline.openTimestamp, 1672515780000ULL, "kline.openTimestamp");
        checkEqual(kline.closeTimestamp, 1672515839999ULL, "kline.closeTimestamp");
        checkEqual(kline.firstTradeId, 100ULL, "kline.firstTradeId");
        checkEqual(kline.lastTradeId, 200ULL, "kline.lastTradeId");

        checkEqual(kline.openPrice, 100'000, "kline.openPrice");
        checkEqual(kline.closePrice, 110'000, "kline.closePrice");
        checkEqual(kline.highPrice, 120'000, "kline.highPrice");
        checkEqual(kline.lowPrice, 90'000, "kline.lowPrice");

        checkEqual(kline.volume, 100'000'000'000LL, "kline.volume");
        checkEqual(kline.tradeCount, 101ULL, "kline.tradeCount");
        check(!kline.closed, "kline.closed");
    }

    void testParseTicker()
    {
        const std::string message = readFile(dataDir () / "ticker.json");
        const binance::Ticker ticker = binance::BinanceDataParser::parseTicker(message);

        checkEqual(ticker.symbol, "BNBBTC", "ticker.symbol");
        checkEqual(ticker.priceChange, 100'000, "ticker.priceChange");
        checkEqual(ticker.weightedAveragePrice, 1'000'000, "ticker.weightedAveragePrice");
        checkEqual(ticker.previousClosePrice, 900'000, "ticker.previousClosePrice");
        checkEqual(ticker.lastPrice, 1'000'000, "ticker.lastPrice");
        checkEqual(ticker.lastQuantity, 10'000'000'000LL, "ticker.lastQuantity");

        checkEqual(ticker.bidPrice, 990'000, "ticker.bidPrice");
        checkEqual(ticker.bidQuantity, 1'000'000'000LL, "ticker.bidQuantity");
        checkEqual(ticker.askPrice, 1'010'000, "ticker.askPrice");
        checkEqual(ticker.askQuantity, 2'000'000'000LL, "ticker.askQuantity");

        checkEqual(ticker.openPrice, 900'000, "ticker.openPrice");
        checkEqual(ticker.highPrice, 1'100'000, "ticker.highPrice");
        checkEqual(ticker.lowPrice, 800'000, "ticker.lowPrice");

        checkEqual(ticker.volume, 100'000'000'000LL, "ticker.volume");
        checkEqual(ticker.quoteAssetVolume, 1'000'000'000LL, "ticker.quoteAssetVolume");

        checkEqual(ticker.firstTradeId, 100ULL, "ticker.firstTradeId");
        checkEqual(ticker.lastTradeId, 200ULL, "ticker.lastTradeId");
        checkEqual(ticker.tradeCount, 101ULL, "ticker.tradeCount");
        checkEqual(ticker.exchangeTimestamp, 1672515782136ULL, "ticker.exchangeTimestamp");
    }
}

int main([[maybe_unused]] int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    using namespace tests;

    try
    {
        testParseTrade();
        testParseAggregateTrade();
        testParseBookTicker();
        testParseKline();
        testParseTicker();

        std::cout << "All BinanceMarketDataParser tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return EXIT_SUCCESS;
}

