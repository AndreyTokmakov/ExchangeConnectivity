/**============================================================================
Name        : types.cpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : types.cpp
============================================================================**/

#include "types.hpp"

namespace trading::common
{
    using namespace std::string_view_literals;

    [[nodiscard]]
    std::string_view toString(const Side side) noexcept
    {
        switch (side)
        {
            case Side::Buy  : return "BUY"sv;
            case Side::Sell : return "SELL"sv;
        }
        return {};
    }
}
