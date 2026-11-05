/**============================================================================
Name        : order.hpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : order.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_ORDER_HPP
#define EXCHANGECONNECTIVITY_ORDER_HPP

#include "../common/price.hpp"
#include "../common/quantity.hpp"
#include "../common/timestamp.hpp"
#include "../common/side.hpp"

namespace trading
{
    using common::Price;
    using common::Quantity;
    using common::InstrumentId;
    using common::Side;

    using OrderId         = uint64_t;
    using ExchangeOrderId = uint64_t;

    enum class OrderType : uint8_t
    {
        Market,
        Limit
    };

    enum class OrderStatus : uint8_t
    {
        New,
        PartiallyFilled,
        Filled,
        Cancelled,
        Rejected
    };

    enum class ExecType : uint8_t
    {
        New,
        Trade,
        Cancel,
        Reject
    };

    struct OrderRequest
    {
        InstrumentId instrument {};
        Side side { Side::Buy };
        OrderType type { OrderType::Limit };
        Price price { 0 };
        Quantity quantity { 0};
    };

    /*
        Order represents an order tracked by the execution subsystem.
        OrderManager owns the lifecycle of the order and updates its execution
        state when ExecutionReport instances are received.
    */
    struct Order
    {
        OrderId clientOrderId { 0 };
        ExchangeOrderId exchangeOrderId { 0 };
        InstrumentId instrument { 0 };
        Side side { Side::Buy };
        OrderType type { OrderType::Limit };
        Price price {};
        Quantity quantity {};
        Quantity filledQuantity {};
        OrderStatus status { OrderStatus::New };
    };

    struct OrderSubmission
    {
        OrderId clientOrderId { 0 };
        ExchangeOrderId exchangeOrderId { 0 };
        OrderStatus status { OrderStatus::New };
    };

    enum class OrderSubmissionErrorCode : std::uint8_t
    {
        InvalidInstrument,
        InvalidOrder,
        SigningFailed,
        TransportError,
        ExchangeRejected,
        InvalidResponse
    };

    struct OrderSubmissionError
    {
        OrderSubmissionErrorCode code { OrderSubmissionErrorCode::InvalidOrder };
        std::int64_t exchangeErrorCode { 0 };
        std::string message;
    };

    [[nodiscard]]
    std::string_view toString(OrderType type) noexcept;

    [[nodiscard]]
    OrderStatus toOrderStatus(std::string_view status) noexcept;

}

#endif //EXCHANGECONNECTIVITY_ORDER_HPP
