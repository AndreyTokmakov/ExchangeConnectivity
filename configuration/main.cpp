/**============================================================================
Name        : Utilities.cpp
Created on  : 29.02.2024
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : C++ Utilities
============================================================================**/

#include <iostream>
#include <string_view>
#include <vector>

#include "configuration_parser.hpp"

namespace
{
    constexpr std::filesystem::path dataDir() noexcept
    {
        return std::filesystem::current_path() / "../../binance/config/";
    }
}

int main([[maybe_unused]] int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    using namespace configuration;

    const std::filesystem::path file = dataDir() / "testnet.json";
    const ConfigurationResult result = ConfigurationParser::parse(file);
    if (!result) {
        std::cerr << result.error().message << '\n';
        return 1;
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
    std::cout << "  Transport: " << binance.orderEntry.transport << '\n';
    std::cout << "  Host: " << binance.orderEntry.endpoint.host << '\n';
    std::cout << "  Port: " << binance.orderEntry.endpoint.port << '\n';

    std::cout << "Instruments:\n";

    for (auto const& instrument : binance.instruments)
        std::cout << "  " << instrument << '\n';

    std::cout << "API Key: " << binance.credentials.apiKey << '\n';
    std::cout << "Secret: " << binance.credentials.secret << '\n';


    return EXIT_SUCCESS;
}
