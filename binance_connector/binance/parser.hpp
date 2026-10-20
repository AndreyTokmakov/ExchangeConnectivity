/**============================================================================
Name        : parser.hpp
Created on  : 20.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : parser.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_PARSER_HPP
#define EXCHANGECONNECTIVITY_BINANCE_PARSER_HPP

#include "types.hpp"
#include <string_view>

namespace binance
{
    class Parser
    {
    public:
        [[nodiscard]]
        static bool isSubscriptionResponse(std::string_view message);

        [[nodiscard]]
        static DepthUpdate parseDepthUpdate(std::string_view message);

        [[nodiscard]]
        static Trade parseTrade(std::string_view message);

        [[nodiscard]]
        static Snapshot parseSnapshot(std::string_view message);
    };
}


#endif //EXCHANGECONNECTIVITY_BINANCE_PARSER_HPP
