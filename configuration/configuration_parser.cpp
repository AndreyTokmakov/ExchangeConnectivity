/**============================================================================
Name        : configuration_parser.cpp
Created on  : 11.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : configuration_parser.cpp
============================================================================**/

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "configuration_parser.hpp"

namespace configuration
{
    namespace
    {
        using Json = nlohmann::json;

        [[nodiscard]]
        std::string resolveEnvironmentVariable(std::string const& value)
        {
            if (!value.starts_with("${") || !value.ends_with('}'))
                return value;

            auto const name = value.substr(2, value.size() - 3);
            auto const* environmentVariable = std::getenv(name.c_str());

            if (environmentVariable == nullptr)
            {
                std::cerr << "Environment variable is not set: "<< name << '\n';
                return {};
            }

            return environmentVariable;
        }

        [[nodiscard]]
        Endpoint parseEndpoint(Json const& json)
        {
            return {
                .host = json.at("host").get<std::string>(),
                .port = json.at("port").get<uint16_t>()
            };
        }

        [[nodiscard]]
        Credentials parseCredentials(Json const& json)
        {
            return {
                .apiKey = resolveEnvironmentVariable(json.at("apiKey").get<std::string>()),
                .secret = resolveEnvironmentVariable(json.at("secret").get<std::string>())
            };
        }

        [[nodiscard]]
        MarketData parseMarketData(Json const& json)
        {
            return {
                .transport = json.at("transport").get<std::string>(),
                .endpoint = parseEndpoint(json),
                .channels = json.at("channels").get<std::vector<std::string>>()
            };
        }

        [[nodiscard]]
        OrderEntry parseOrderEntry(Json const& json)
        {
            return {
                .transport = json.at("transport").get<std::string>(),
                .endpoint = parseEndpoint(json)
            };
        }

        [[nodiscard]]
        Connector parseConnector(Json const& json)
        {
            return {
                .type = json.at("type").get<std::string>(),
                .marketData = parseMarketData(json.at("marketData")),
                .orderEntry = parseOrderEntry(json.at("orderEntry")),
                .credentials = parseCredentials(json.at("credentials")),
                .instruments =json.at("instruments").get<std::vector<std::string>>()
            };
        }
    }

    ConfigurationResult
    ConfigurationParser::parse(std::filesystem::path const& path)
    {
        std::ifstream file{path};

        if (!file){
            return std::unexpected(ConfigurationError{"Unable to open configuration file: " + path.string()});
        }

        try
        {
            auto const json = Json::parse(file);
            if (json.at("version").get<int>() != 1) {
                return std::unexpected(ConfigurationError{"Unsupported configuration version"});
            }

            Configuration configuration;
            for (auto const& [name, connectorJson] :json.at("connectors").items()) {
                configuration.connectors_.emplace(name,parseConnector(connectorJson));
            }

            return configuration;
        }
        catch (Json::exception const& error)
        {
            return std::unexpected(ConfigurationError{"Invalid configuration: " + std::string{error.what()}});
        }
    }
}