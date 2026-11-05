/**============================================================================
Name        : utilities.cpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : utilities.cpp
============================================================================**/

#include "utilities.hpp"

namespace
{
    using namespace std::string_view_literals;

    constexpr std::string_view HexDigits = "0123456789ABCDEF"sv;
}

namespace utilities
{
    std::string urlEncode(const std::string_view value)
    {
        std::string result;
        result.reserve(value.size());

        for (const unsigned char character : value)
        {
            if (std::isalnum(character) || character == '-' || character == '_' || character == '.' || character == '~')
            {
                result.push_back(static_cast<char>(character));
            }
            else
            {
                result.push_back('%');
                result.push_back(HexDigits[character >> 4]);
                result.push_back(HexDigits[character & 0x0F]);
            }
        }
        return result;
    }

    std::string toHex(const std::span<const unsigned char> data)
    {
        std::string result;
        result.reserve(data.size() * 2);
        for (const unsigned char value : data) {
            result.push_back(HexDigits[value >> 4]);
            result.push_back(HexDigits[value & 0x0F]);
        }
        return result;
    }

    consteval int64_t pow10(const int exponent) noexcept
    {
        int64_t result { 1 };
        for (int index { 0 }; index < exponent; ++index)
            result *= 10;
        return result;
    }
}
