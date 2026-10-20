/**============================================================================
Name        : connector.cpp
Created on  : 20.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : connector.cpp
============================================================================**/

#include "connector.hpp"

#include "parser.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    [[nodiscard]]
    std::string toLower(std::string value)
    {
        for (char& character : value) {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }

        return value;
    }
}

namespace binance
{
    Connector::Connector(boost::asio::io_context& ioContext, std::string symbol)
        : ioContext { ioContext },
          sslContext { boost::asio::ssl::context::tls_client },
          restClient { ioContext, sslContext },
          symbol { toLower(std::move(symbol)) }
    {
        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(boost::asio::ssl::verify_peer);
    }

    Connector::~Connector()
    {
        stop();
    }

    void Connector::startDepth(DepthHandler handler)
    {
        stopDepth();
        depthHandler = std::move(handler);

        depthClient = std::make_unique<WebSocketClient>(ioContext, sslContext, symbol + "@depth@100ms");
        depthClient->start([this](const std::string& message) {
            onDepthMessage(message);
        });
    }

    void Connector::startTrades(TradeHandler handler)
    {
        stopTrades();
        tradeHandler = std::move(handler);

        tradeClient = std::make_unique<WebSocketClient>(ioContext, sslContext, symbol + "@trade");
        tradeClient->start([this](const std::string& message){
            onTradeMessage(message);
        });
    }

    void Connector::asyncSnapshot(SnapshotHandler handler)
    {
        restClient.asyncGetDepthSnapshot(symbol,
            [this, handler = std::move(handler)](RestClient::Response response) mutable {
            onSnapshotResponse(std::move(response), handler);
        });
    }

    void Connector::stopDepth()
    {
        if (depthClient) {
            depthClient->stop();
            depthClient.reset();
        }
        depthHandler = {};
    }

    void Connector::stopTrades()
    {
        if (tradeClient) {
            tradeClient->stop();
            tradeClient.reset();
        }
        tradeHandler = {};
    }

    void Connector::stop()
    {
        stopDepth();
        stopTrades();
    }

    void Connector::onDepthMessage(const std::string& message) const
    {
        try {
            if (Parser::isSubscriptionResponse(message)) {
                return;
            }

            const DepthUpdate update = Parser::parseDepthUpdate(message);

            if (depthHandler) {
                depthHandler(update);
            }
        } catch (const std::exception& exception) {
            std::cerr << "Failed to parse depth update: " << exception.what() << '\n';
        }
    }

    void Connector::onTradeMessage(const std::string& message) const
    {
        try {
            if (Parser::isSubscriptionResponse(message)) {
                return;
            }

            const Trade trade = Parser::parseTrade(message);

            if (tradeHandler) {
                tradeHandler(trade);
            }
        } catch (const std::exception& exception) {
            std::cerr << "Failed to parse trade: " << exception.what() << '\n';
        }
    }

    void Connector::onSnapshotResponse(RestClient::Response response,
                                               const SnapshotHandler& handler) const
    {
        if (!response) {
            handler(std::unexpected(response.error()));
            return;
        }

        try {
            handler(Parser::parseSnapshot(*response));
        } catch (const std::exception& exception) {
            handler(std::unexpected(RestFailure {
                RestError::InvalidResponse, exception.what()
            }));
        }
    }
}
