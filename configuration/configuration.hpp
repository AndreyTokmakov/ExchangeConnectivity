/**============================================================================
Name        : configuration.hpp
Created on  : 11.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : configuration.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_CONFIGURATION_HPP
#define EXCHANGECONNECTIVITY_CONFIGURATION_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace configuration
{
    struct Endpoint
    {
        std::string host;
        uint16_t port {};
    };

    struct Credentials
    {
        std::string apiKey;
        std::string secret;
    };

    struct MarketData
    {
        std::string transport;
        Endpoint endpoint;
        std::vector<std::string> channels;
    };

    struct OrderEntry
    {
        std::string transport;
        Endpoint endpoint;
    };

    struct Connector
    {
        std::string type;
        MarketData marketData;
        OrderEntry orderEntry;
        Credentials credentials;
        std::vector<std::string> instruments;
    };

    class Configuration
    {
    public:
        using Connectors = std::unordered_map<std::string, Connector>;

        [[nodiscard]]
        Connector const& connector(const std::string_view name) const
        {
            return connectors_.at(std::string{name});
        }

        [[nodiscard]]
        Connectors const& connectors() const noexcept
        {
            return connectors_;
        }

    private:
        friend class ConfigurationParser;

        Connectors connectors_;
    };
}

#endif //EXCHANGECONNECTIVITY_CONFIGURATION_HPP
