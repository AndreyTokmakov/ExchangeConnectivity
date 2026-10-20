/**============================================================================
Name        : connector.hpp
Created on  : 20.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : connector.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_CONNECTOR_HPP
#define EXCHANGECONNECTIVITY_BINANCE_CONNECTOR_HPP

#include "rest_client.hpp"
#include "types.hpp"
#include "websocket_client.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <functional>
#include <memory>
#include <string>

namespace binance
{
    class BinanceConnector
    {
    public:
        using DepthHandler = std::function<void(const DepthUpdate&)>;
        using TradeHandler = std::function<void(const Trade&)>;
        using SnapshotHandler = std::function<void(std::expected<Snapshot, RestFailure>)>;

        BinanceConnector(boost::asio::io_context& ioContext, std::string symbol);
        ~BinanceConnector();

        BinanceConnector(const BinanceConnector&) = delete;
        BinanceConnector& operator=(const BinanceConnector&) = delete;

        void startDepth(DepthHandler handler);
        void startTrades(TradeHandler handler);
        void asyncSnapshot(SnapshotHandler handler);

        void stopDepth();
        void stopTrades();
        void stop();

    private:
        void onDepthMessage(const std::string& message) const;
        void onTradeMessage(const std::string& message) const;
        void onSnapshotResponse(RestClient::Response response, const SnapshotHandler& handler) const;

        boost::asio::io_context& ioContext;
        boost::asio::ssl::context sslContext;
        RestClient restClient;
        std::string symbol;

        std::unique_ptr<WebSocketClient> depthClient;
        std::unique_ptr<WebSocketClient> tradeClient;

        DepthHandler depthHandler;
        TradeHandler tradeHandler;
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_CONNECTOR_HPP
