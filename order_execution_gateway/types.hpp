/**============================================================================
Name        : types.hpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : types.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_TYPES_HPP
#define EXCHANGECONNECTIVITY_TYPES_HPP

#include <string>
#include <string_view>
#include <chrono>

namespace trading::common
{
    using InstrumentId    = uint32_t;
    using SequenceNumber  = uint64_t;

    consteval int64_t pow10(const int exponent) noexcept
    {
        int64_t result { 1 };
        for (int index { 0 }; index < exponent; ++index)
            result *= 10;
        return result;
    }

    template<typename Derived, int DecimalPlacesCount = 8>
    class ScaledValue
    {
    public:
        using Value = int64_t;

        static constexpr int DecimalPlaces = DecimalPlacesCount;
        static constexpr Value Scale = pow10(DecimalPlaces);

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

    class Price final : public ScaledValue<Price>
    {
    public:
        using ScaledValue<Price>::ScaledValue;
    };

    class Quantity final : public ScaledValue<Quantity>
    {
    public:
        using ScaledValue<Quantity>::ScaledValue;
    };

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

    class Timestamp
    {
    public:
        using Value = uint64_t;

        constexpr Timestamp() noexcept = default;

        explicit constexpr Timestamp(const Value nanoseconds) noexcept: nanoseconds_ { nanoseconds } {
        }

        [[nodiscard]]
        static Timestamp now() noexcept
        {
            const auto duration = std::chrono::steady_clock::now().time_since_epoch();
            return Timestamp { static_cast<Value>( std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()) };
        }

        [[nodiscard]]
        constexpr Value nanoseconds() const noexcept {
            return nanoseconds_;
        }

        [[nodiscard]]
        constexpr Timestamp operator+(const uint64_t nanoseconds) const noexcept {
            return Timestamp { nanoseconds_ + nanoseconds };
        }

        [[nodiscard]]
        constexpr uint64_t operator-(const Timestamp other) const noexcept {
            return nanoseconds_ - other.nanoseconds_;
        }

        constexpr auto operator<=>(const Timestamp&) const noexcept = default;

    private:
        Value nanoseconds_ { 0 };
    };

    enum class Side : uint8_t
    {
        Buy,
        Sell
    };
}

#endif //EXCHANGECONNECTIVITY_TYPES_HPP

