/**============================================================================
Name        : binance_spot_client.cpp
Created on  : 03.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : binance_spot_client.cpp
============================================================================**/

#include "binance_spot_client.hpp"
#include "utilities.hpp"
#include "order.hpp"

#include <simdjson.h>
#include <openssl/hmac.h>
#include <boost/beast/http/verb.hpp>

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <format>

namespace
{
    using namespace std::string_view_literals;
    namespace networking = trading::networking;;
    namespace common = trading::common;;

    using common::Price;
    using common::Quantity;
    using common::Side;

    using trading::OrderId;
    using trading::OrderRequest;
    using trading::OrderStatus;
    using trading::OrderType;
    using trading::OrderSubmission;

    using trading::binance::AssetBalance;
    using trading::binance::BinanceApiError;
    using trading::binance::BinanceApiErrorCode;
    using trading::binance::BinanceSpotClient;
    using trading::binance::OrderCancellation;

    constexpr std::string_view OrderEndpoint = "/api/v3/order";
    constexpr std::string_view AccountEndpoint = "/api/v3/account";

    [[nodiscard]]
    std::uint64_t unixTimestampMilliseconds() noexcept
    {
        const auto duration = std::chrono::system_clock::now().time_since_epoch();

        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
        );
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

        if (fractionalPart != 0) {
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

        std::int64_t signedValue = 0;
        if (!json[key].get_int64().get(signedValue) && signedValue >= 0) {
            output = static_cast<std::uint64_t>(signedValue);
            return true;
        }

        return false;
    }

    [[nodiscard]]
    std::int64_t getJsonInt64(const simdjson::dom::element& json,
                              const std::string_view key)
    {
        std::int64_t value = 0;
        json[key].get_int64().get(value);
        return value;
    }

    [[nodiscard]]
    BinanceSpotClient::OrderResponse parseOrderResponse(const networking::HttpResponse& response,
                                                        const OrderId clientOrderId)
    {
        simdjson::dom::parser parser;
        simdjson::dom::element json;

        const simdjson::error_code errorCode = parser.parse(response.body).get(json);
        if (errorCode != simdjson::SUCCESS || !json.is_object()) {
            return std::unexpected {
                makeError(BinanceApiErrorCode::InvalidResponse,
                          "Invalid Binance order response")
            };
        }

        if (response.status < 200 || response.status >= 300)
        {
            const std::string message = [&json]() {
                std::string value;
                getJsonString(json, "msg", value);
                return value;
            }();

            return std::unexpected {makeError(BinanceApiErrorCode::ExchangeRejected,
                          message.empty() ? "Binance rejected order" : message,
                          getJsonInt64(json, "code"))
            };
        }

        std::uint64_t exchangeOrderId = 0;
        std::string status;

        if (!getJsonUint64(json, "orderId", exchangeOrderId) || !getJsonString(json, "status", status) ||
            exchangeOrderId == 0)
        {
            return std::unexpected {makeError(BinanceApiErrorCode::InvalidResponse,
                          "Binance response does not contain orderId or status")
            };
        }

        const std::optional<OrderStatus> orderStatus = trading::toOrderStatus(status);
        if (!orderStatus) {
            return std::unexpected { makeError(BinanceApiErrorCode::InvalidResponse,
                          std::format("Unknown Binance order status: {}", status))
            };
        }

        return OrderSubmission {
            .clientOrderId = clientOrderId,
            .exchangeOrderId = exchangeOrderId,
            .status = *orderStatus
        };
    }

    [[nodiscard]]
    BinanceSpotClient::CancelResponse parseCancelResponse(const networking::HttpResponse& response)
    {
        simdjson::dom::parser parser;
        simdjson::dom::element json;

        const simdjson::error_code errorCode = parser.parse(response.body).get(json);
        if (errorCode != simdjson::SUCCESS || !json.is_object()) {
            return std::unexpected { makeError(BinanceApiErrorCode::InvalidResponse,
                          "Invalid Binance cancel response")
            };
        }

        if (response.status < 200 || response.status >= 300) {
            std::string message;
            getJsonString(json, "msg", message);

            return std::unexpected { makeError(BinanceApiErrorCode::ExchangeRejected,
                          message.empty() ? "Binance rejected cancellation" : message,
                          getJsonInt64(json, "code"))
            };
        }

        std::uint64_t exchangeOrderId = 0;
        std::string status;

        if (!getJsonUint64(json, "orderId", exchangeOrderId) ||
            !getJsonString(json, "status", status) ||
            exchangeOrderId == 0)
        {
            return std::unexpected {makeError(BinanceApiErrorCode::InvalidResponse,
                          "Binance cancel response does not contain orderId or status")
            };
        }

        const std::optional<OrderStatus> orderStatus = trading::toOrderStatus(status);
        if (!orderStatus) {
            return std::unexpected {makeError(BinanceApiErrorCode::InvalidResponse,
                          std::format("Unknown Binance order status: {}", status))
            };
        }

        return OrderCancellation {
            .exchangeOrderId = exchangeOrderId,
            .status = *orderStatus
        };
    }

    [[nodiscard]]
    BinanceSpotClient::BalancesResponse parseBalancesResponse(const networking::HttpResponse& response)
    {
        simdjson::dom::parser parser;
        simdjson::dom::element json;

        const simdjson::error_code errorCode = parser.parse(response.body).get(json);
        if (errorCode != simdjson::SUCCESS || !json.is_object()) {
            return std::unexpected {
                makeError(BinanceApiErrorCode::InvalidResponse,
                          "Invalid Binance account response")
            };
        }

        if (response.status < 200 || response.status >= 300)
        {
            std::string message;
            getJsonString(json, "msg", message);

            return std::unexpected {makeError(BinanceApiErrorCode::ExchangeRejected,
                          message.empty() ? "Binance rejected account request" : message,
                          getJsonInt64(json, "code"))
            };
        }

        simdjson::dom::array balances;

        if (json["balances"].get_array().get(balances)) {
            return std::unexpected {
                makeError(BinanceApiErrorCode::InvalidResponse,
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
                    makeError(BinanceApiErrorCode::InvalidResponse,
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
    BinanceSpotClient::BinanceSpotClient(networking::RestClient& restClient,
                                         ApiCredentials credentials,
                                         std::string host,
                                         const std::uint16_t port,
                                         std::string symbol,
                                         const std::uint64_t recvWindow):
        restClient { restClient },
        credentials { std::move(credentials) },
        host { std::move(host) },
        port { port },
        symbol { std::move(symbol) },
        recvWindow { recvWindow }
    {
    }

    void BinanceSpotClient::asyncSignedRequest(const boost::beast::http::verb method,
                                               std::string endpoint,
                                               std::string query,
                                               RestResponseHandler handler)
    {
        if (!handler) {
            return;
        }
        if (credentials.apiKey.empty() || credentials.secretKey.empty())
        {
            handler(std::unexpected { networking::RestFailure {
                    .error = networking::RestError::InvalidRequest,
                    .message = "Binance API credentials are empty"
                }
            });
            return;
        }

        if (!query.empty()) {
            query += '&';
        }

        query += std::format("recvWindow={}&timestamp={}", recvWindow, unixTimestampMilliseconds());
        const std::string signature = createSignature(query, credentials.secretKey);

        if (signature.empty())
        {
            handler(std::unexpected { networking::RestFailure {
                    .error = networking::RestError::InvalidRequest,
                    .message = "Failed to create HMAC SHA256 signature"
                }
            });
            return;
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

        restClient.asyncRequest(httpRequest, std::move(handler));
    }

    void BinanceSpotClient::sendOrder(const OrderRequest& request,
                                      const OrderId clientOrderId,
                                      OrderHandler handler)
    {
        if (!handler) {
            return;
        }

        const std::string_view side = toString(request.side);
        const std::string_view type = toString(request.type);

        if (clientOrderId == 0 || request.quantity.raw() <= 0 || side.empty() || type.empty() ||
            (request.type == OrderType::Limit && request.price.raw() <= 0))
        {
            handler(std::unexpected {makeError(BinanceApiErrorCode::InvalidRequest,"Invalid order parameters")});
            return;
        }

        std::string query = std::format("symbol={}&side={}&type={}&quantity={}&newClientOrderId={}",
            utilities::urlEncode(symbol),
            side,
            type,
            ::toString(request.quantity),
            clientOrderId
        );


        if (request.type == OrderType::Limit) {
            query += std::format("&price={}&timeInForce=GTC", ::toString(request.price));
        }

        asyncSignedRequest(boost::beast::http::verb::post,
            std::string { OrderEndpoint },
            std::move(query),
            [handler = std::move(handler), clientOrderId](RestResponse response) mutable
            {
                if (!response) {
                    handler(std::unexpected {makeError(BinanceApiErrorCode::TransportError, response.error().message)});
                    return;
                }

                handler(parseOrderResponse(*response, clientOrderId));
            }
        );
    }

    void BinanceSpotClient::cancelOrder(const ExchangeOrderId exchangeOrderId,
                                        CancelHandler handler)
    {
        if (!handler) {
            return;
        }
        if (exchangeOrderId == 0) {
            handler(std::unexpected {makeError(BinanceApiErrorCode::InvalidRequest, "Invalid exchange order ID")});
            return;
        }

        const std::string query = std::format("symbol={}&orderId={}",
            utilities::urlEncode(symbol),
            exchangeOrderId
        );

        asyncSignedRequest(boost::beast::http::verb::delete_,
            std::string { OrderEndpoint },
            query,
            [handler = std::move(handler)](RestResponse response) mutable
            {
                if (!response) {
                    handler(std::unexpected { makeError(BinanceApiErrorCode::TransportError, response.error().message)});
                    return;
                }
                handler(parseCancelResponse(*response));
            }
        );
    }

    void BinanceSpotClient::getBalances(BalancesHandler handler)
    {
        if (!handler) {
            return;
        }

        asyncSignedRequest(boost::beast::http::verb::get,
            std::string { AccountEndpoint },
            {},
            [handler = std::move(handler)](RestResponse response) mutable
            {
                if (!response) {
                    handler(std::unexpected { makeError(BinanceApiErrorCode::TransportError, response.error().message)});
                    return;
                }

                handler(parseBalancesResponse(*response));
            }
        );
    }
}