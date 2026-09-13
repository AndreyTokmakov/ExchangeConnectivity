/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/


#include <iostream>
#include <print>
#include <format>
#include <string_view>
#include <array>
#include <utility>
#include <vector>
#include <filesystem>

#include <boost/asio/io_context.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <nlohmann/json.hpp>
#include "configuration_parser.hpp"

#include "binance_web_socket_client.hpp"
#include "binance_market_data_parser.hpp"
#include "binance_rest_client.hpp"

namespace
{
    constexpr std::filesystem::path dataDir() noexcept
    {
        return std::filesystem::current_path() / "../../binance/config/";
    }
}

namespace
{
    void configTest()
    {
        using namespace configuration;
        const std::filesystem::path file = dataDir() / "testnet.json";
        const ConfigurationResult result = ConfigurationParser::parse(file);
        if (!result) {
            std::cerr << result.error().message << '\n';
            return;
        }

        const Configuration & configuration = *result;
        const Connector &binance = configuration.connector("binance");

        std::cout << "Connector: " << binance.type << '\n';

        std::cout << "Market Data:\n";
        std::cout << "  Transport: " << binance.marketData.transport << '\n';
        std::cout << "  Host: " << binance.marketData.endpoint.host << '\n';
        std::cout << "  Port: " << binance.marketData.endpoint.port << '\n';

        std::cout << "  Channels:\n";

        for (auto const& channel : binance.marketData.channels)
            std::cout << "    " << channel << '\n';

        std::cout << "Order Entry:\n";
        std::cout << "   Transport: " << binance.orderEntry.transport << '\n';
        std::cout << "   Host: " << binance.orderEntry.endpoint.host << '\n';
        std::cout << "   Port: " << binance.orderEntry.endpoint.port << '\n';

        std::cout << "Instruments:\n";

        for (auto const& instrument : binance.instruments)
            std::cout << "   " << instrument << '\n';

        std::cout << "API Key: " << binance.credentials.apiKey << '\n';
        std::cout << "Secret: " << binance.credentials.secret << '\n';
    }

    void wsTest()
    {

        const std::string host { "stream.binance.com" };
        constexpr uint16_t port { 443 };

        constexpr std::string_view subscriptionDepth {
            R"({"method": "SUBSCRIBE","params": ["btcusdt@depth"], "id": 1})"
        };

        constexpr exchange::binance::BinanceMarketDataParser parser;

        boost::asio::io_context ioContext;
        boost::asio::ssl::context sslContext{boost::asio::ssl::context::tls_client};

        int msgCount = 0;

        exchange::binance::BinanceWebSocketClient webSocket(ioContext,sslContext,
            [&](const std::string_view message) {
                std::cout << "Received: " << message << '\n';
                ++msgCount;
                if (msgCount > 1) {
                    const exchange::binance::DepthUpdate update = parser.parseDepthUpdate(message);
                    std::cout << update.asks.size() << '\n';
                    std::cout << update.bids.size() << '\n';
                }
            },
            [](const boost::beast::error_code &errorCode){
                std::cerr << "WebSocket error: " << errorCode.message()<< '\n';
        });

        webSocket.connect(host, std::to_string(port),"/ws");
        webSocket.send(subscriptionDepth);
        ioContext.run();
    }

    void apiClientTest()
    {
        using namespace exchange::binance;

        BinanceRestClient restClient { "testnet.binance.vision" };

        const std::expected<DepthSnapshot, RestError> result = restClient.getDepthSnapshot("BTCUSDT", 1000);
        if (!result)
        {
            std::cerr << "Failed to get Binance depth snapshot: "<< static_cast<int>(result.error())<< '\n';
            return;
        }

        const DepthSnapshot& snapshot = *result;

        std::cout << "Last update ID: " << snapshot.lastUpdateId << '\n';
        std::cout << "Bids: " << snapshot.bids.size() << '\n';
        std::cout << "Asks: " << snapshot.asks.size()<< '\n';
    }
}


int main([[maybe_unused]] int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    // configTest();
    // wsTest();
    apiClientTest();

    return EXIT_SUCCESS;
}

