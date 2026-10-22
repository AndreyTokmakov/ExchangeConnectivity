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
#include <thread>

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
}

int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    boost::asio::io_context ioContext;
    boost::asio::ssl::context sslContext { boost::asio::ssl::context::tls_client };
    sslContext.set_default_verify_paths();
    sslContext.set_verify_mode(boost::asio::ssl::verify_peer);

    networking::WebSocketClient client {
        ioContext,
        sslContext,
        "stream.binance.com",
        "443",
        "/ws"
    };

    client.start([](const std::string& message) {
        std::cout << message << '\n';
    });

    std::jthread ioThread { [&ioContext] {
        ioContext.run();
    } };

    // Wait until the WebSocket connection is established.
    // The current WebSocketClient API does not expose connection status.
    std::this_thread::sleep_for(std::chrono::seconds { 1 });

    client.send(R"({
        "method": "SUBSCRIBE",
        "params": ["btcusdt@depth"],
        "id": 1
    })");

    std::cout << "Subscribed to BTCUSDT depth stream. Press Enter to stop.\n";
    std::cin.get();

    client.stop();
    ioContext.stop();

    return EXIT_SUCCESS;
}

