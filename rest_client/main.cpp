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
#include <thread>

#include "rest_client.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http.hpp>

namespace
{
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace asio  = boost::asio;
    namespace ssl   = asio::ssl;

    using namespace std::string_view_literals;

    constexpr std::string_view HexDigits = "0123456789ABCDEF"sv;

    [[nodiscard]]
    std::string urlEncode(const std::string_view value)
    {
        std::string result;
        result.reserve(value.size());

        for (const unsigned char character : value)
        {
            if (std::isalnum(character) || character == '-' || character == '_' ||
                character == '.' || character == '~') {
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


    class TestRestClient
    {
    public:
        explicit TestRestClient(std::string endpoint) noexcept :
            endpoint { std::move(endpoint) },
            ioContext { 1 },
            sslContext { boost::asio::ssl::context::tls_client },
            workGuard { ioContext.get_executor() },
            restClient { ioContext, sslContext },
            executorThread { [this] { ioContext.run(); } }
        {

        }

        [[nodiscard]]
        bool sendRequestAsync()
        {
            networking::HttpRequest request;
            request.host = endpoint;
            request.target = "/api/v3/depth?symbol=" + urlEncode("BTCUSDT") + "&limit=1000";
            request.method = boost::beast::http::verb::get;

            restClient.asyncRequest(request,[](networking::RestClient::Response response) mutable
            {
                if (!response) {
                    std::cerr << response.error().message << std::endl;
                    return;
                }

                std::cout << response->status << std::endl;
                std::cout << response->body << std::endl;
                /*
                if (!parseDepthSnapshot(response->body, snapshot)) {
                    std::cerr << "Parse depth snapshot failed" << std::endl;
                }*/
            });

            return true;
        }

        [[nodiscard]]
        bool sendRequest()
        {
            networking::HttpRequest request;
            request.host = endpoint;
            request.target = "/api/v3/depth?symbol=" + urlEncode("BTCUSDT") + "&limit=1000";
            request.method = http::verb::get;

            const auto response  = restClient.request(request);
            std::cout << response.value().body << std::endl;

            return true;
        }

    private:
        using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

        std::string endpoint;
        asio::io_context ioContext;
        ssl::context sslContext;
        WorkGuard workGuard;
        networking::RestClient restClient;
        std::jthread executorThread;
    };

    void run()
    {
        TestRestClient client {  "testnet.binance.vision" };
        for (int i = 0; i < 3; ++i) {
            std::cout << std::string(140, '=') << std::endl;
            const auto _ = client.sendRequest();
            // const auto _ = client.sendRequestAsync();
            std::cout << std::string(140, '=') << std::endl;
        }

    }
}



int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    run();

    return EXIT_SUCCESS;
}

