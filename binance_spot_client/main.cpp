/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/

#include <string_view>
#include <vector>
#include <iostream>

#include "binance_spot_client.hpp"
#include "rest_client.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http.hpp>

#include "order.hpp"

namespace
{
    using namespace trading::common;
    using namespace trading::binance;
    using namespace trading::networking;

    using trading::binance::ApiCredentials;


    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace asio  = boost::asio;
    namespace ssl   = asio::ssl;

    constexpr std::string_view Host = "testnet.binance.vision";
    constexpr std::string_view Port = "443";
    constexpr std::string_view OrderEndpoint = "/api/v3/order";

    std::optional<std::string> getEnvironmentVariable(const std::string_view variable) {
        if (const char* env_p = std::getenv(variable.data()))
            return env_p;
        return std::nullopt;
    }

    [[nodiscard]]
    std::optional<ApiCredentials> loadApiCredentials()
    {
        return ApiCredentials {
            .apiKey = getEnvironmentVariable("BINANCE_API_KEY").value_or(""),
            .secretKey = getEnvironmentVariable("BINANCE_SECRET_KEY").value_or("")
        };
    }

    void run()
    {
        const std::optional<ApiCredentials> credentials = loadApiCredentials();

        if (!credentials) {
            std::cerr << "Binance API credentials are not configured\n";
            return;
        }

        asio::io_context ioContext;

        ssl::context sslContext { ssl::context::tls_client };
        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(ssl::verify_peer);

        RestClient restClient { ioContext, sslContext };
        BinanceSpotClient client { restClient, *credentials };

        trading::OrderRequest request {
            .instrument = 0,
            .side = Side::Buy,
            .type = trading::OrderType::Limit,
            .price = Price::fromInteger(75'000),
            .quantity = Quantity { 100'000 } // 0.001 BTC
        };

        constexpr trading::OrderId clientOrderId = 10001;

        /*
        client.sendOrder(request, clientOrderId,[](BinanceSpotClient::OrderResponse response)
        {
            if (!response) {
                std::cerr << "Order submission failed: " << response.error().message << '\n';
                return;
            }
            std::cout << "Order submitted\n"
                      << "Exchange order ID: " << response->exchangeOrderId << '\n'
                      << "Status: " << static_cast<int>(response->status) << '\n';
        });*/

        /*
        client.getBalances([](BinanceSpotClient::BalancesResponse response)
        {
            if (!response) {
                std::cerr << "Balance request failed: " << response.error().message << '\n';
                return;
            }
            for (const AssetBalance& balance : *response) {
                std::cout << balance.asset << " | free: " << balance.free << " | locked: " << balance.locked << '\n';
            }
        });
        */


        constexpr uint64_t exchangeOrderId = 10887393;
        client.cancelOrder(exchangeOrderId,[](BinanceSpotClient::CancelResponse response)
        {
            if (!response) {
                std::cerr << "Order cancellation failed: " << response.error().message << '\n';
                return;
            }
            std::cout << "Order cancelled\n" << "Exchange order ID: " << response->exchangeOrderId << '\n';
        });

        ioContext.run();
    }
}

int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    run();

    return EXIT_SUCCESS;
}
