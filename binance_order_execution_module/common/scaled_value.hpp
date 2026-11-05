/**============================================================================
Name        : scaled_value.hpp
Created on  : 20.08.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : scaled_value.hpp
============================================================================**/


#ifndef FINANCETECHNOLOGYPROJECTS_SCALED_VALUE_HPP
#define FINANCETECHNOLOGYPROJECTS_SCALED_VALUE_HPP

#include "../utilities/utilities.hpp"

namespace common
{
    template<typename Derived, int DecimalPlacesCount = 8>
    class ScaledValue
    {
    public:
        using Value = int64_t;

        static constexpr int DecimalPlaces = DecimalPlacesCount;
        static constexpr Value Scale = utilities::pow10(DecimalPlaces);

        constexpr ScaledValue() noexcept = default;

        explicit constexpr ScaledValue(const Value value) noexcept:
            value { value }
        {
        }

        [[nodiscard]]
        static constexpr Derived fromInteger(const Value value) noexcept
        {
            return Derived { value * Scale };
        }

        [[nodiscard]]
        constexpr Value raw() const noexcept
        {
            return value;
        }

        [[nodiscard]]
        constexpr bool isZero() const noexcept
        {
            return value == 0;
        }

        [[nodiscard]]
        constexpr bool isPositive() const noexcept
        {
            return value > 0;
        }

        [[nodiscard]]
        constexpr bool isNegative() const noexcept
        {
            return value < 0;
        }

        [[nodiscard]]
        constexpr Derived operator+(const Derived& other) const noexcept
        {
            return Derived { value + other.raw() };
        }

        [[nodiscard]]
        constexpr Derived operator-(const Derived& other) const noexcept
        {
            return Derived { value - other.raw() };
        }

        [[nodiscard]]
        constexpr Derived operator*(const Value multiplier) const noexcept
        {
            return Derived { value * multiplier };
        }

        constexpr Derived& operator+=(const Derived& other) noexcept
        {
            value += other.raw();
            return derived();
        }

        constexpr Derived& operator-=(const Derived& other) noexcept
        {
            value -= other.raw();
            return derived();
        }

        constexpr auto operator<=>(const ScaledValue&) const noexcept = default;

    private:
        [[nodiscard]]
        constexpr Derived& derived() noexcept
        {
            return static_cast<Derived&>(*this);
        }

        Value value { 0 };
    };
}

#endif //FINANCETECHNOLOGYPROJECTS_SCALED_VALUE_HPP