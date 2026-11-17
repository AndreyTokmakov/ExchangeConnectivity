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
    /**
     * URL Encoding Function.
     *
     * This function encodes a given string into its URL-encoded format,
     * replacing non-ASCII characters with their corresponding hexadecimal escapes.
     *
     * @param value The input string to be encoded. It is assumed that the input string is in UTF-8 encoding.
     * @return The URL-encoded string.
    */
    [[nodiscard]]
    std::string urlEncode(std::string_view value);

    [[nodiscard]]
    std::string toHex(std::span<const unsigned char> data);

    [[nodiscard]]
    consteval int64_t pow10(int exponent) noexcept;

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
