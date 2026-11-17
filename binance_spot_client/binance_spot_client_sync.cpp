/**============================================================================
Name        : binance_spot_client_sync.cpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_spot_client_sync.cpp
============================================================================**/

#include "binance_spot_client_sync.hpp"
#include "utilities.hpp"
#include "order.hpp"

#include <simdjson.h>
#include <openssl/hmac.h>
#include <boost/beast/http/verb.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <iomanip>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    namespace networking = trading::networking;
    namespace common = trading::common;

    using common::Price;
    using common::Quantity;
    using common::Side;

    using trading::OrderId;
    using trading::OrderRequest;
    using trading::OrderStatus;
    using trading::OrderType;
    using trading::OrderSubmission;

    using trading::binance::OrderResponse;
    using trading::binance::CancelResponse;
    using trading::binance::BalancesResponse;
    using trading::binance::AssetBalance;
    using trading::binance::BinanceApiError;
    using trading::binance::BinanceApiErrorCode;
    using trading::binance::OrderCancellation;
    using trading::binance::spot_client_synch::BinanceSpotClient;


    constexpr std::string_view OrderEndpoint = "/api/v3/order";
    constexpr std::string_view AccountEndpoint = "/api/v3/account";

    [[nodiscard]]
    std::string toDecimalString(const std::int64_t value, const int decimalPlaces)
    {
        const bool negative = value < 0;
        const std::uint64_t absoluteValue = negative ? static_cast<std::uint64_t>(-(value + 1)) + 1
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
            stream << '.' << std::setfill('0') << std::setw(decimalPlaces) << fractionalPart;
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
        unsigned int digestLength = 0;

        const unsigned char* result = HMAC(EVP_sha256(),
                                           secretKey.data(),
                                           static_cast<int>(secretKey.size()),
                                           reinterpret_cast<const unsigned char*>(payload.data()),
                                           payload.size(),
                                           digest.data(),
                                           &digestLength);
        if (nullptr == result) {
            return {};
        }

        return utilities::toHex(std::span { digest.data(), digestLength });
    }

    [[nodiscard]]
    BinanceApiError makeError(const BinanceApiErrorCode code,
                              std::string message,
                              const std::int64_t exchangeErrorCode = 0)
    {
        return BinanceApiError {
            .code = code,
            .exchangeErrorCode = exchangeErrorCode,
            .message = std::move(message)
        };
    }

    [[nodiscard]]
    bool getJsonString(const simdjson::dom::element& json,
                       const std::string_view key,
                       std::string& output)
    {
        std::string_view value;
        if (json[key].get_string().get(value)) {
            return false;
        }

        output.assign(value);
        return true;
    }

    [[nodiscard]]
    bool getJsonUint64(const simdjson::dom::element& json,
                       const std::string_view key,
                       std::uint64_t& output)
    {
        if (!json[key].get_uint64().get(output)) {
            return true;
        }
        if (std::int64_t signedValue = 0; !json[key].get_int64().get(signedValue) && signedValue >= 0) {
            output = static_cast<std::uint64_t>(signedValue);
            return true;
        }

        return false;
    }

    [[nodiscard]]
    bool getJsonInt64(const simdjson::dom::element& json,
                      const std::string_view key,
                      std::int64_t& output)
    {
        return !json[key].get_int64().get(output);
    }

    [[nodiscard]]
    std::string createOrderQuery(const OrderRequest& request,
                                 const OrderId clientOrderId,
                                 const std::string_view symbol)
    {
        std::string query = std::format("symbol={}&side={}&type={}&quantity={}&newClientOrderId={}",
            utilities::urlEncode(symbol),
            toString(request.side),
            toString(request.type),
            toString(request.quantity),
            clientOrderId);

        if (request.type == OrderType::Limit) {
            query += std::format("&price={}&timeInForce=GTC", toString(request.price));
        }

        return query;
    }

    [[nodiscard]]
    OrderResponse parseOrderResponse(const networking::HttpResponse& response,
                                                        const OrderId clientOrderId)
    {
        simdjson::dom::parser parser;
        simdjson::dom::element json;

        const simdjson::error_code errorCode = parser.parse(response.body).get(json);
        if (errorCode != simdjson::SUCCESS || !json.is_object()) {
            return std::unexpected {
                makeError(
                    BinanceApiErrorCode::InvalidResponse,
                    "Invalid Binance order response")
            };
        }

        if (response.status < 200 || response.status >= 300)
        {
            std::int64_t exchangeErrorCode = 0;
            std::string message;

            const bool hasErrorCode = getJsonInt64(json, "code", exchangeErrorCode);
            const bool hasMessage = getJsonString(json, "msg", message);

            if (!hasErrorCode || !hasMessage) {
                return std::unexpected {makeError(BinanceApiErrorCode::InvalidResponse,"Invalid Binance error response")};
            }

            return std::unexpected {makeError(
                BinanceApiErrorCode::ExchangeRejected,
                message.empty() ? "Binance rejected order" : std::move(message),
                exchangeErrorCode)
            };
        }

        std::uint64_t exchangeOrderId = 0;
        std::string status;

        if (!getJsonUint64(json, "orderId", exchangeOrderId) ||
            !getJsonString(json, "status", status) ||
            exchangeOrderId == 0)
        {
            return std::unexpected {makeError(
                    BinanceApiErrorCode::InvalidResponse,
                    "Binance response does not contain orderId or status")
            };
        }

        const OrderStatus orderStatus = trading::toOrderStatus(status);
        if (OrderStatus::Unknown == orderStatus) {
            return std::unexpected { makeError(
                BinanceApiErrorCode::InvalidResponse,
                 std::format("Unknown Binance order status: {}", status))
            };
        }

        return OrderSubmission {
            .clientOrderId = clientOrderId,
            .exchangeOrderId = exchangeOrderId,
            .status = orderStatus
        };
    }

    [[nodiscard]]
    CancelResponse parseCancelResponse(const networking::HttpResponse& response)
    {
        simdjson::dom::parser parser;
        simdjson::dom::element json;

        const simdjson::error_code errorCode = parser.parse(response.body).get(json);
        if (errorCode != simdjson::SUCCESS || !json.is_object()) {
            return std::unexpected { makeError(
                BinanceApiErrorCode::InvalidResponse,
                "Invalid Binance cancel response")
            };
        }

        if (response.status < 200 || response.status >= 300) {
            std::int64_t exchangeErrorCode = 0;
            std::string message;

            const bool hasErrorCode = getJsonInt64(json, "code", exchangeErrorCode);
            const bool hasMessage = getJsonString(json, "msg", message);

            if (!hasErrorCode || !hasMessage) {
                return std::unexpected {makeError(BinanceApiErrorCode::InvalidResponse,"Invalid Binance error response")};
            }

            return std::unexpected { makeError(
                BinanceApiErrorCode::ExchangeRejected,
                message.empty() ? "Binance rejected cancellation" : std::move(message),
                exchangeErrorCode)
            };
        }

        std::uint64_t exchangeOrderId = 0;
        std::string status;

        if (!getJsonUint64(json, "orderId", exchangeOrderId) ||
            !getJsonString(json, "status", status) ||
            exchangeOrderId == 0)
        {
            return std::unexpected { makeError(
                BinanceApiErrorCode::InvalidResponse,
                "Binance cancel response does not contain orderId or status")
            };
        }

        const OrderStatus orderStatus = trading::toOrderStatus(status);
        if (OrderStatus::Unknown == orderStatus) {
            return std::unexpected { makeError(
                BinanceApiErrorCode::InvalidResponse,
                 std::format("Unknown Binance order status: {}", status))
            };
        }

        return OrderCancellation {
            .exchangeOrderId = exchangeOrderId,
            .status = orderStatus
        };
    }

    [[nodiscard]]
    BalancesResponse parseBalancesResponse(const networking::HttpResponse& response)
    {
        simdjson::dom::parser parser;
        simdjson::dom::element json;

        const simdjson::error_code errorCode = parser.parse(response.body).get(json);
        if (errorCode != simdjson::SUCCESS || !json.is_object()) {
            return std::unexpected {
                makeError(
                    BinanceApiErrorCode::InvalidResponse,
                    "Invalid Binance account response")
            };
        }

        if (response.status < 200 || response.status >= 300) {
            std::int64_t exchangeErrorCode = 0;
            std::string message;

            const bool hasErrorCode = getJsonInt64(json, "code", exchangeErrorCode);
            const bool hasMessage = getJsonString(json, "msg", message);

            if (!hasErrorCode || !hasMessage) {
                return std::unexpected {makeError(BinanceApiErrorCode::InvalidResponse,"Invalid Binance error response")};
            }
            return std::unexpected { makeError(
                    BinanceApiErrorCode::ExchangeRejected,
                    message.empty() ? "Binance rejected account request" : std::move(message),
                    exchangeErrorCode)
            };
        }

        simdjson::dom::array balances;

        if (json["balances"].get_array().get(balances)) {
            return std::unexpected {
                makeError(
                    BinanceApiErrorCode::InvalidResponse,
                    "Binance account response does not contain balances")
            };
        }

        std::vector<AssetBalance> result;
        result.reserve(balances.size());

        for (const simdjson::dom::element balance : balances) {
            AssetBalance item;

            if (!getJsonString(balance, "asset", item.asset) ||
                !getJsonString(balance, "free", item.free) ||
                !getJsonString(balance, "locked", item.locked))
            {
                return std::unexpected {
                    makeError(
                        BinanceApiErrorCode::InvalidResponse,
                        "Invalid balance entry in Binance response")
                };
            }

            result.push_back(std::move(item));
        }

        return result;
    }
}

namespace trading::binance
{
    BinanceSpotClient::BinanceSpotClient(
        ApiCredentials credentials,
        std::string host,
        const std::uint16_t port,
        std::string symbol,
        const std::uint64_t recvWindow)
        : sslContext { boost::asio::ssl::context::tlsv12_client }
        , restClient { ioContext, sslContext }
        , credentials { std::move(credentials) }
        , host { std::move(host) }
        , port { port }
        , symbol { std::move(symbol) }
        , recvWindow { recvWindow }
    {
        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(boost::asio::ssl::verify_peer);
    }

    BinanceSpotClient::ApiResponse
    BinanceSpotClient::signedRequest(const boost::beast::http::verb method,
                                     std::string_view endpoint,
                                     std::string query)
    {
        if (credentials.apiKey.empty() || credentials.secretKey.empty()) {
            return std::unexpected {
                makeError(
                    BinanceApiErrorCode::SigningFailed,
                    "Binance API credentials are empty")
            };
        }

        if (!query.empty()) {
            query += '&';
        }

        query += std::format("recvWindow={}&timestamp={}",recvWindow, utilities::unixTimestampMilliseconds());
        const std::string signature = createSignature(query, credentials.secretKey);
        if (signature.empty()) {
            return std::unexpected { makeError(BinanceApiErrorCode::SigningFailed, "Failed to create HMAC SHA256 signature")};
        }

        networking::HttpRequest httpRequest {
            .host = host,
            .port = std::to_string(port),
            .target = std::format("{}?{}&signature={}", endpoint, query, signature),
            .method = method,
            .headers = {
                { "X-MBX-APIKEY", credentials.apiKey }
            }
        };

        std::optional<ApiResponse> result;

        ioContext.restart();
        restClient.asyncRequest(httpRequest, [&result](RestResponse response) {
            if (!response) {
                result = std::unexpected {makeError(BinanceApiErrorCode::TransportError, response.error().message)};
                return;
            }
            result = std::move(*response);
        });

        ioContext.run();

        if (!result) {
            return std::unexpected {makeError(BinanceApiErrorCode::TransportError, "REST request completed without a response")};
        }
        return std::move(*result);
    }

    OrderResponse BinanceSpotClient::sendOrder(const OrderRequest& request,
                                               const OrderId clientOrderId)
    {
        const std::string_view side = toString(request.side);
        const std::string_view type = toString(request.type);

        if (clientOrderId == 0 || request.quantity.raw() <= 0 ||
            side.empty() || type.empty() ||
            (request.type == OrderType::Limit && request.price.raw() <= 0))
        {
            return std::unexpected {makeError(BinanceApiErrorCode::InvalidRequest, "Invalid order parameters")};
        }

        if (symbol.empty()) {
            return std::unexpected {makeError(BinanceApiErrorCode::InvalidRequest,"Instrument symbol is empty")};
        }

        const std::string query = createOrderQuery(request, clientOrderId, symbol);
        ApiResponse response = signedRequest(boost::beast::http::verb::post, OrderEndpoint ,query);
        if (!response) {
            return std::unexpected { response.error() };
        }

        return parseOrderResponse(*response, clientOrderId);
    }

    CancelResponse BinanceSpotClient::cancelOrder(const ExchangeOrderId exchangeOrderId)
    {
        if (exchangeOrderId == 0) {
            return std::unexpected {makeError(BinanceApiErrorCode::InvalidRequest, "Invalid exchange order ID")};
        }
        if (symbol.empty()) {
            return std::unexpected {makeError(BinanceApiErrorCode::InvalidRequest, "Instrument symbol is empty")};
        }

        const std::string query = std::format("symbol={}&orderId={}", utilities::urlEncode(symbol), exchangeOrderId);
        ApiResponse response = signedRequest(boost::beast::http::verb::delete_, OrderEndpoint , query);
        if (!response) {
            return std::unexpected { response.error() };
        }

        return parseCancelResponse(*response);
    }

    BalancesResponse BinanceSpotClient::getBalances()
    {
        ApiResponse response = signedRequest(boost::beast::http::verb::get, AccountEndpoint,{});
        if (!response) {
            return std::unexpected { response.error() };
        }

        return parseBalancesResponse(*response);
    }
}