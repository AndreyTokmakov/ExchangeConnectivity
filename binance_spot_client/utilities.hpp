/**============================================================================
Name        : utilities.hpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : utilities.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_UTILITIES_HPP
#define EXCHANGECONNECTIVITY_UTILITIES_HPP

#include <string>
#include <string_view>
#include <span>

namespace utilities
{
    [[nodiscard]]
    std::string urlEncode(std::string_view value);

    [[nodiscard]]
    std::string toHex(const std::span<const unsigned char> data);
}

#endif //EXCHANGECONNECTIVITY_UTILITIES_HPP
