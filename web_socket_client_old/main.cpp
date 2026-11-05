/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/

#include <iostream>
#include <syncstream>
#include <print>
#include <format>
#include <string_view>
#include <algorithm>
#include <vector>
#include <filesystem>

#include "web_socket_client.hpp"

namespace
{
    using namespace std::chrono;
    constexpr std::string_view formatMSeconds { "%d-%02d-%02d %02d:%02d:%02d.%06ld" };

    [[nodiscard]]
    std::string getCurrentTime(const time_point<system_clock>& timestamp = system_clock::now())
    {
        const time_t time { system_clock::to_time_t(timestamp) };
        std::tm tm {};
        ::localtime_r(&time, &tm);

        std::string buffer(64, '\0');
        const int32_t size = std::sprintf(buffer.data(), formatMSeconds.data(),
                                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                                          duration_cast<microseconds>(timestamp - time_point_cast<seconds>(timestamp)).count()
        );
        buffer.resize(size);
        buffer.shrink_to_fit();
        return buffer;
    }

    #define LOG  std::osyncstream { std::cout } << getCurrentTime() << " "

    [[nodiscard]]
    std::string toLower(std::string value)
    {
        std::ranges::transform(value, value.begin(),[](const unsigned char character){
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    void onMessage(const std::string_view message)
    {
        LOG << message << std::endl;
    }

}

int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    using networking::WebSocketClient;

    const std::string symbol { "BTCUSDT"  };

    boost::asio::io_context ioContext;
    boost::asio::ssl::context sslContext { boost::asio::ssl::context::tls_client };

    const std::unique_ptr<WebSocketClient> wsDepthClient = std::make_unique<WebSocketClient>(
        ioContext, sslContext, toLower(symbol) + "@depth@100ms"
    );

    wsDepthClient->start([](const std::string& message){
        onMessage(message);
    });

    ioContext.run();

    return EXIT_SUCCESS;
}

