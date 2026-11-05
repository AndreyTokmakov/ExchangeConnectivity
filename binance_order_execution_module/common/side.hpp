/**============================================================================
Name        : side.hpp
Created on  : 05.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : side.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_SIDE_HPP
#define EXCHANGECONNECTIVITY_SIDE_HPP

#include <cstdint>
#include <string_view>

namespace common
{
    using InstrumentId    = uint32_t;
    using SequenceNumber  = uint64_t;

    enum class Side : uint8_t
    {
        Buy,
        Sell
    };

    [[nodiscard]]
    std::string_view toString(Side side) noexcept;
}

#endif //EXCHANGECONNECTIVITY_SIDE_HPP
