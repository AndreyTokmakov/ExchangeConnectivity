/**============================================================================
Name        : order.cpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : order.cpp
============================================================================**/

#include "order.hpp"

namespace trading
{
    using namespace std::string_view_literals;

    [[nodiscard]]
    std::string_view toString(const OrderType type) noexcept
    {
        switch (type)
        {
            case OrderType::Market:
                return "MARKET"sv;
            case OrderType::Limit:
                return "LIMIT"sv;
        }
        return {};
    }
}
