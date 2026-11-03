/**============================================================================
Name        : binance_order_gateway.cpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_order_gateway.cpp
============================================================================**/

#include "binance_order_gateway.hpp"
#include "utilities.hpp"

#include <openssl/hmac.h>
#include <boost/json.hpp>
#include <boost/beast/http/verb.hpp>

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    using namespace std::string_view_literals;
    namespace networking = trading::networking;;
    namespace common = trading::common;;

    using common::Side;
    using common::Price;
    using common::Quantity;
    using trading::OrderType;
    using trading::OrderRequest;
    using trading::OrderId;
    using trading::OrderSubmission;
    using trading::OrderSubmissionError;
    using trading::OrderSubmissionErrorCode;
    using trading::OrderStatus;
    using trading::binance_order_gateway::BinanceOrderGateway;

    constexpr std::uint64_t RecvWindow = 5000;

    [[nodiscard]]
    std::uint64_t unixTimestampMilliseconds() noexcept
    {
        const auto duration = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    }

    [[nodiscard]]
    std::string toDecimalString(const std::int64_t value, const int decimalPlaces)
    {
        const bool negative = value < 0;

        // Avoid overflow for INT64_MIN.
        const std::uint64_t absoluteValue = negative
            ? static_cast<std::uint64_t>(-(value + 1)) + 1
            : static_cast<std::uint64_t>(value);

        std::uint64_t scale = 1;
        for (int index = 0; index < decimalPlaces; ++index) {
            scale *= 10;
        }

        const std::uint64_t integerPart = absoluteValue / scale;
        const std::uint64_t fractionalPart = absoluteValue % scale;

        std::ostringstream stream;

        if (negative) {
            stream << '-';
        }

        stream << integerPart;
        if (fractionalPart != 0)
        {
            stream << '.' << std::setfill('0')<< std::setw(decimalPlaces) << fractionalPart;
            std::string result = stream.str();
            while (result.back() == '0') {
                result.pop_back();
            }
            if (result.back() == '.') {
                result.pop_back();
            }
            return result;
        }

        return stream.str();
    }

    [[nodiscard]]
    std::string toString(const Price& price)
    {
        return toDecimalString(price.raw(), Price::DecimalPlaces);
    }

    [[nodiscard]]
    std::string toString(const Quantity& quantity)
    {
        return toDecimalString(quantity.raw(), Quantity::DecimalPlaces);
    }

    [[nodiscard]]
    std::string createSignature(const std::string_view payload,
                                const std::string_view secretKey)
    {
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest {};
        uint32_t digestLength = 0;

        const unsigned char* result = HMAC(
            EVP_sha256(),
            secretKey.data(),
            static_cast<int>(secretKey.size()),
            reinterpret_cast<const unsigned char*>(payload.data()),
            payload.size(),
            digest.data(),
            &digestLength
        );

        if (nullptr == result) {
            return {};
        }

        std::string signature;
        signature.reserve(digestLength * 2);

        for (unsigned int index = 0; index < digestLength; ++index)
        {
            constexpr char Hex[] = "0123456789abcdef";
            signature.push_back(Hex[digest[index] >> 4]);
            signature.push_back(Hex[digest[index] & 0x0F]);
        }

        return signature;
    }

    [[nodiscard]]
    std::string createQuery(const OrderRequest& request,
                            const OrderId clientOrderId,
                            const std::string_view symbol)
    {
        std::ostringstream query;
        query << "symbol=" << utilities::urlEncode(symbol)
              << "&side=" << toString(request.side)
              << "&type=" << toString(request.type)
              << "&quantity=" << toString(request.quantity)
              << "&newClientOrderId=" << clientOrderId;

        if (request.type == OrderType::Limit) {
            query << "&price=" << toString(request.price)<< "&timeInForce=GTC";
        }
        query << "&recvWindow=" << RecvWindow << "&timestamp=" << unixTimestampMilliseconds();

        return query.str();
    }

    [[nodiscard]]
    OrderSubmissionError makeError(const OrderSubmissionErrorCode code,
                                   std::string message,
                                   const std::int64_t exchangeErrorCode = 0)
    {
        return OrderSubmissionError {
            .code = code,
            .exchangeErrorCode = exchangeErrorCode,
            .message = std::move(message)
        };
    }

    [[nodiscard]]
    std::string getJsonString(const boost::json::value& json,
                              const std::string_view key)
    {
        if (!json.is_object()) {
            return {};
        }

        const boost::json::value* value = json.as_object().if_contains(key);
        if (nullptr == value || !value->is_string()) {
            return {};
        }

        return std::string(value->as_string());
    }

    [[nodiscard]]
    std::uint64_t getJsonUint64(const boost::json::value& json,
                                const std::string_view key)
    {
        if (!json.is_object()) {
            return 0;
        }

        const boost::json::value* value = json.as_object().if_contains(key);
        if (nullptr == value) {
            return 0;
        }
        if (value->is_uint64()) {
            return value->as_uint64();
        }
        if (value->is_int64() && value->as_int64() >= 0) {
            return static_cast<std::uint64_t>(value->as_int64());
        }

        return 0;
    }

    [[nodiscard]]
    BinanceOrderGateway::Response parseResponse(const trading::networking::HttpResponse& response,
                                                const OrderId clientOrderId)
    {
        boost::system::error_code errorCode;

        const boost::json::value json = boost::json::parse(response.body, errorCode);

        if (errorCode || !json.is_object()) {
            return std::unexpected { makeError(
                OrderSubmissionErrorCode::InvalidResponse,
                "Invalid Binance response: " + errorCode.message()) };
        }

        if (response.status < 200 || response.status >= 300)
        {
            const std::int64_t exchangeErrorCode =
                json.as_object().if_contains("code") &&
                json.as_object().at("code").is_int64()
                    ? json.as_object().at("code").as_int64()
                    : 0;

            const std::string message = getJsonString(json, "msg");

            return std::unexpected { makeError(
                OrderSubmissionErrorCode::ExchangeRejected,
                message.empty() ? "Binance rejected order" : message,
                exchangeErrorCode) };
        }

        const std::uint64_t exchangeOrderId = getJsonUint64(json, "orderId");
        const std::string status = getJsonString(json, "status");

        if (exchangeOrderId == 0 || status.empty()) {
            return std::unexpected { makeError(
                OrderSubmissionErrorCode::InvalidResponse,
                "Binance response does not contain orderId or status") };
        }

        return OrderSubmission {
            .clientOrderId = clientOrderId,
            .exchangeOrderId = exchangeOrderId,
            .status = trading::toOrderStatus(status)
        };
    }
}

namespace trading::binance_order_gateway
{
    BinanceOrderGateway::BinanceOrderGateway(networking::RestClient& restClient,
                                             ApiCredentials credentials,
                                             std::string host,
                                             const uint16_t port,
                                             std::string order_endpoint,
                                             std::string symbol):
        restClient { restClient },
        credentials { std::move(credentials) },
        host { std::move(host) },
        port { port },
        orderEndpoint { std::move(order_endpoint)},
        symbol { std::move(symbol)}
    {
    }
    void BinanceOrderGateway::sendOrder(const OrderRequest& request,
                                        const OrderId clientOrderId,
                                        ResponseHandler handler)
    {
        if (!handler) {
            return;
        }
        if (clientOrderId == 0 || request.quantity.raw() <= 0 ||
            (request.type == OrderType::Limit && request.price.raw() <= 0))
        {
            handler(std::unexpected { makeError(OrderSubmissionErrorCode::InvalidOrder,"Invalid order parameters") });
            return;
        }

        if (credentials.apiKey.empty() || credentials.secretKey.empty())
        {
            handler(std::unexpected { makeError(OrderSubmissionErrorCode::SigningFailed,"Binance API credentials are empty") });
            return;
        }

        if (symbol.empty())
        {
            handler(std::unexpected { makeError(OrderSubmissionErrorCode::InvalidInstrument,"Unknown instrument") });
            return;
        }

        const std::string query = createQuery(request, clientOrderId, symbol);
        const std::string signature = createSignature(query, credentials.secretKey);

        if (signature.empty())
        {
            handler(std::unexpected { makeError(OrderSubmissionErrorCode::SigningFailed,"Failed to create HMAC SHA256 signature") });
            return;
        }

        networking::HttpRequest httpRequest {
            .host = host ,
            .port = std::to_string(port),
            .target = std::format("{}?{}&signature={}", orderEndpoint, query, signature),
            .method = boost::beast::http::verb::post,
            .headers = {
                { "X-MBX-APIKEY", credentials.apiKey }
            }
        };

        restClient.asyncRequest(httpRequest,[handler = std::move(handler), clientOrderId](networking::RestClient::Response response) mutable
        {
            if (!response) {
                handler(std::unexpected { makeError(OrderSubmissionErrorCode::TransportError,response.error().message) });
                return;
            }

            handler(parseResponse(*response, clientOrderId));
        });
    }
}