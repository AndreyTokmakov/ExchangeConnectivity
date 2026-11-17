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
#include <thread>

#include "binance_order_gateway.hpp"
#include "network_runtime.hpp"
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

    using trading::binance::ApiCredentials;

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
        NetworkRuntime networkRuntime;
        const std::optional<ApiCredentials> credentials = loadApiCredentials();
        if (!credentials) {
            std::cerr << "Binance API credentials are not configured\n";
            return;
        }

      spot_client_asynch::BinanceOrderGateway orderGateway {networkRuntime,  "testnet.binance.vision", *credentials };

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
        orderGateway.sendOrder(request, clientOrderId,[](OrderResponse response)
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
        constexpr uint64_t exchangeOrderId = 15011447;
        orderGateway.cancelOrder(exchangeOrderId,[](CancelResponse response)
        {
            if (!response) {
                std::cerr << "Order cancellation failed: " << response.error().message << '\n';
                return;
            }
            std::cout << "Order cancelled\n" << "Exchange order ID: " << response->exchangeOrderId << '\n';
        });
#endif

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    runAsynch();

    return EXIT_SUCCESS;
}
