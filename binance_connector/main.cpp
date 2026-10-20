/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/

#include "connector.hpp"
#include <iostream>
#include <boost/asio/steady_timer.hpp>

int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    boost::asio::io_context ioContext;
    binance::BinanceConnector connector { ioContext, "BTCUSDT" };

    std::uint64_t depthUpdateCount = 0;
    std::uint64_t tradeCount = 0;

    connector.startDepth([&depthUpdateCount](const binance::DepthUpdate& update)
    {
        ++depthUpdateCount;
        std::cout << "Depth:"
                  << " firstUpdateId=" << update.firstUpdateId
                  << ", lastUpdateId=" << update.lastUpdateId
                  << ", bids=" << update.bids.size()
                  << ", asks=" << update.asks.size()
                  << '\n';
    });

    connector.startTrades([&tradeCount](const binance::Trade& trade)
    {
        ++tradeCount;
        std::cout << "Trade:"
                  << " id=" << trade.sequence
                  << ", price=" << static_cast<double>(trade.price) / binance::DecimalScale
                  << ", quantity="
                  << static_cast<double>(trade.quantity) / binance::DecimalScale
                  << ", side=" << (trade.side == binance::Side::Buy ? "Buy" : "Sell")
                  << '\n';
    });

    boost::asio::steady_timer snapshotTimer { ioContext, std::chrono::seconds { 10 } };
    snapshotTimer.async_wait([&connector](const boost::system::error_code& errorCode)
    {
        if (errorCode) {
            return;
        }

        connector.asyncSnapshot([](std::expected<binance::Snapshot, binance::RestFailure> result)
        {
            if (!result) {
                std::cerr << "Snapshot failed: " << result.error().message << '\n';
                return;
            }
            std::cout << "Snapshot:"
                      << " lastUpdateId=" << result->lastUpdateId
                      << ", bids=" << result->bids.size()
                      << ", asks=" << result->asks.size()
                      << '\n';
        });
    });

    boost::asio::steady_timer stopTimer { ioContext, std::chrono::seconds { 30 } };

    stopTimer.async_wait([&connector](const boost::system::error_code& errorCode)
    {
        if (!errorCode) {
            connector.stop();
        }
    });

    ioContext.run();

    std::cout << "Depth updates received: " << depthUpdateCount << '\n';
    std::cout << "Trades received: " << tradeCount << '\n';


    return EXIT_SUCCESS;
}

