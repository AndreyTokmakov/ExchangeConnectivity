/**============================================================================
Name        : timestamp.hpp
Created on  : 15.08.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : timestamp.hpp
============================================================================**/

#ifndef FINANCETECHNOLOGYPROJECTS_TIMESTAMP_HPP
#define FINANCETECHNOLOGYPROJECTS_TIMESTAMP_HPP

#include <chrono>
#include <compare>
#include <cstdint>

namespace common
{
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

}

#endif //FINANCETECHNOLOGYPROJECTS_TIMESTAMP_HPP
