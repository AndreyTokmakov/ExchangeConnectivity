/**============================================================================
Name        : side.cpp
Created on  : 05.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : side.cpp
============================================================================**/

#include "side.hpp"

namespace common
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