/**============================================================================
Name        : configuration_parser.hpp
Created on  : 11.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : configuration_parser.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_CONFIGURATION_PARSER_HPP
#define EXCHANGECONNECTIVITY_CONFIGURATION_PARSER_HPP

#include "configuration.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace configuration
{
    struct ConfigurationError
    {
        std::string message;
    };

    using ConfigurationResult = std::expected<Configuration, ConfigurationError>;

    class ConfigurationParser
    {
    public:
        [[nodiscard]]
        static ConfigurationResult parse(std::filesystem::path const& path);
    };
}

#endif //EXCHANGECONNECTIVITY_CONFIGURATION_PARSER_HPP
