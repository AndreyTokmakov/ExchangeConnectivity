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

#include "rest_client.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include "binance_api.hpp"
#include "binance_spot_client.hpp"

namespace
{
    using namespace trading::common;
    using namespace trading::networking;

    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace asio  = boost::asio;
    namespace ssl   = asio::ssl;

    constexpr std::string_view Host = "testnet.binance.vision";
    constexpr std::string_view Port = "443";
    constexpr std::string_view OrderEndpoint = "/api/v3/order";

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
}

int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    std::optional<ApiCredentials> credentials = loadApiCredentials();
    if (!credentials) {
        std::cerr << "Binance API credentials are not configured\n";
        return EXIT_FAILURE;
    }

    asio::io_context ioContext;

    ssl::context sslContext { ssl::context::tls_client };
    sslContext.set_default_verify_paths();
    sslContext.set_verify_mode(ssl::verify_peer);

    trading::binance::spot_client::BinanceSpotClient client {
        ioContext,
        sslContext,
        std::move(credentials.value())
    };

    boost::asio::co_spawn(ioContext,client.getBalances(), [](const std::exception_ptr& exception,
           trading::binance::BalancesResponse response)
    {
        if (exception) {
            std::cerr << "Unexpected exception\n";
            return;
        }
        if (!response) {
            std::cerr << "Binance error: " << response.error().message << '\n';
            return;
        }
        for (const auto& balance : *response) {
            std::cout << balance.asset << " | Free: " << balance.free << " | Locked: " << balance.locked << '\n';
        }
    });

    ioContext.run();

    return EXIT_SUCCESS;
}
