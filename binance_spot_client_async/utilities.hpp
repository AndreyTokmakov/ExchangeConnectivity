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

#include <string_view>
#include <span>
#include <cstdint>

namespace utilities
{
    [[nodiscard]]
    std::string urlEncode(std::string_view value);

    [[nodiscard]]
    std::string toHex(std::span<const unsigned char> data);

    /**
     * Returns the current Unix timestamp in milliseconds.
     *
     * This function calculates the number of milliseconds since the Unix epoch (January 1, 1970).
     * It uses the `std::chrono` library to get the current time and calculate the difference from the epoch.
     *
     * @return The current Unix timestamp in milliseconds as a 64-bit integer.
     */
    [[nodiscard]]
    int64_t unixTimestampMilliseconds();

    [[nodiscard]]
    std::string escapeJsonString(std::string_view value);
}

#endif //EXCHANGECONNECTIVITY_UTILITIES_HPP
