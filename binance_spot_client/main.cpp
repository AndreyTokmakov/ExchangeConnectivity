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
#include "binance_spot_client_sync.hpp"
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

    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace asio  = boost::asio;
    namespace ssl   = asio::ssl;

    constexpr std::string_view Host = "testnet.binance.vision";
    constexpr std::string_view Port = "443";
    constexpr std::string_view OrderEndpoint = "/api/v3/order";

    using trading::binance::ApiCredentials;

    using BinanceSpotClientAsync = trading::binance::spot_client_asynch::BinanceSpotClient;
    using BinanceSpotClientSync  = trading::binance::spot_client_synch::BinanceSpotClient;

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

    void runAsynch()
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
        BinanceSpotClientAsync asynchClient { restClient, *credentials };

        [[maybe_unused]]
        const trading::OrderRequest request {
            .instrument = 0,
            .side = Side::Buy,
            .type = trading::OrderType::Limit,
            .price = Price::fromInteger(55'000),
            .quantity = Quantity { 100'000 } // 0.001 BTC
        };

        [[maybe_unused]]
        constexpr trading::OrderId clientOrderId = 10001;

#if 0
        asynchClient.sendOrder(request, clientOrderId,[](OrderResponse response)
        {
            if (!response) {
                std::cerr << "Order submission failed: " << response.error().message << '\n';
                return;
            }
            std::cout << "Order submitted\n"
                      << "Exchange order ID: " << response->exchangeOrderId << '\n'
                      << "Status: " << static_cast<int>(response->status) << '\n';
        });
#endif
#if 1
        constexpr uint64_t exchangeOrderId = 13905131;
        asynchClient.cancelOrder(exchangeOrderId,[](CancelResponse response)
        {
            if (!response) {
                std::cerr << "Order cancellation failed: " << response.error().message << '\n';
                return;
            }
            std::cout << "Order cancelled\n" << "Exchange order ID: " << response->exchangeOrderId << '\n';
        });
#endif
#if 0
        asynchClient.getBalances([](BalancesResponse response)
        {
            if (!response) {
                std::cerr << "Balance request failed: " << response.error().message << '\n';
                return;
            }
            for (const AssetBalance& balance : *response) {
                std::cout << balance.asset << " | free: " << balance.free << " | locked: " << balance.locked << '\n';
            }
        });
#endif

        ioContext.run();
    }

    void runSynch()
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

        BinanceSpotClientSync client { *credentials };

        [[maybe_unused]]
        trading::OrderRequest request {
            .instrument = 0,
            .side = Side::Buy,
            .type = trading::OrderType::Limit,
            .price = Price::fromInteger(55'000),
            .quantity = Quantity { 100'000 } // 0.001 BTC
        };

        [[maybe_unused]]
        constexpr trading::OrderId clientOrderId = 10001;

#if 1
        const BalancesResponse balances = client.getBalances();
        if (balances.has_value())
        {
            for (const AssetBalance& balance : balances.value()) {
                std::cout << balance.asset << " | free: " << balance.free << " | locked: " << balance.locked << '\n';
            }
        }
#endif
#if 0
        const OrderResponse orderResponse = client.sendOrder(request, clientOrderId);
        if (orderResponse.has_value()) {
            std::cout << "Order submitted\n"
                   << "Client order ID  : " << orderResponse->clientOrderId   << '\n'
                   << "Exchange order ID: " << orderResponse->exchangeOrderId << '\n'
                   << "Status: " << static_cast<int>(orderResponse->status)   << '\n';
        }
#endif
#if 1
        constexpr uint64_t exchangeOrderId = 13720986;
        const CancelResponse cancelResponse = client.cancelOrder(exchangeOrderId);
        if (cancelResponse.has_value()) {
            std::cout << "Order cancelled\n" << "Exchange order ID: " << cancelResponse->exchangeOrderId << '\n';
        } else {
            std::cerr << "Error " << cancelResponse.error().message << '\n';
        }
#endif
        ioContext.run();
    }
}

int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    // runSynch();
    runAsynch();

    return EXIT_SUCCESS;
}
