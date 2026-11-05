/**============================================================================
Name        : instrument.hpp
Created on  : 15.08.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : instrument.hpp
============================================================================**/

#ifndef FINANCETECHNOLOGYPROJECTS_INSTRUMENT_HPP
#define FINANCETECHNOLOGYPROJECTS_INSTRUMENT_HPP

#include <string_view>

#include "price.hpp"
#include "quantity.hpp"

namespace common
{
    class Instrument
    {
    public:
        constexpr Instrument() noexcept = default;

        constexpr Instrument(const InstrumentId id,
                             const std::string_view symbol,
                             const Price tickSize,
                             const Quantity lotSize) noexcept :
            id_ { id },
            symbol_ { symbol },
            tickSize_ { tickSize },
            lotSize_ { lotSize }
        {
        }

        [[nodiscard]]
        constexpr InstrumentId id() const noexcept {
            return id_;
        }

        [[nodiscard]]
        constexpr std::string_view symbol() const noexcept {
            return symbol_;
        }

        [[nodiscard]]
        constexpr Price tickSize() const noexcept {
            return tickSize_;
        }

        [[nodiscard]]
        constexpr Quantity lotSize() const noexcept {
            return lotSize_;
        }

    private:
        InstrumentId id_ { 0 };
        std::string_view symbol_;
        Price tickSize_;
        Quantity lotSize_;
    };
}

#endif //FINANCETECHNOLOGYPROJECTS_INSTRUMENT_HPP
