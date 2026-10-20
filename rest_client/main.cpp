/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/

#include <iostream>
#include <print>
#include <format>
#include <string_view>
#include <utility>
#include <vector>
#include <filesystem>

#include "rest_client.hpp"

namespace binance
{
    constexpr char HexDigits[] = "0123456789ABCDEF";

    [[nodiscard]]
    static std::string urlEncode(const std::string_view value)
    {

        std::string result;
        result.reserve(value.size());

        for (const unsigned char character : value)
        {
            if (std::isalnum(character) || character == '-' || character == '_' || character == '.' || character == '~') {
                result.push_back(static_cast<char>(character));
            }
            else
            {
                result.push_back('%');
                result.push_back(HexDigits[character >> 4]);
                result.push_back(HexDigits[character & 0x0F]);
            }
        }

        return result;
    }

    class BinanceRestClient
    {
    public:
        using Response = std::expected<std::string, networking::RestFailure>;
        using ResponseHandler = std::function<void(Response)>;

        explicit BinanceRestClient(networking::RestClient& restClient);

        BinanceRestClient(const BinanceRestClient&) = delete;
        BinanceRestClient& operator=(const BinanceRestClient&) = delete;

        void asyncGetDepthSnapshot(const std::string& symbol, ResponseHandler handler) const;

    private:
        networking::RestClient& restClient;
    };


    BinanceRestClient::BinanceRestClient(networking::RestClient& restClient)
      : restClient(restClient)
    {
    }

    void BinanceRestClient::asyncGetDepthSnapshot(const std::string& symbol, ResponseHandler handler) const
    {
        networking::HttpRequest request;
        request.host = "api.binance.com";
        request.target = "/api/v3/depth?symbol=" + urlEncode(symbol) + "&limit=1000";
        request.method = boost::beast::http::verb::get;

        restClient.asyncRequest(std::move(request),
            [handler = std::move(handler)](networking::RestClient::Response response) mutable
            {
                if (!handler) {
                    return;
                }

                if (!response) {
                    handler(std::unexpected { response.error() });
                    return;
                }

                if (response->status < 200 || response->status >= 300) {
                    handler(std::unexpected {
                        networking::RestFailure {
                            .error = networking::RestError::HttpStatus,
                            .message = "Binance REST request returned HTTP status " +
                                std::to_string(response->status)
                        }
                    });
                    return;
                }

                handler(Response { std::move(response->body) });
            });
    }
}



int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);


    return EXIT_SUCCESS;
}

