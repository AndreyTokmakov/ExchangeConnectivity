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

#include <boost/asio/io_context.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>

#include "rest_client.hpp"


namespace
{
    using networking::RestFailure;

    void apiClientTest()
    {
        networking::RestClient restClient { "api.binance.com" };
        const std::expected<networking::HttpResponse, networking::RestFailure> response =
            restClient.request(networking::HttpMethod::Get, "/api/v3/time");

        if (!response)
        {
            std::cerr << "REST request failed: " << static_cast<int>(response.error().error) << '\n';
            if (response.error().systemError) {
                std::cerr << "System error: " << response.error().systemError.message() << '\n';
            }
            if (response.error().sslError != 0) {
                std::cerr << "OpenSSL error: " << response.error().sslError << '\n';
            }
            return;
        }

        if (response->status != 200)
        {
            std::cerr << "HTTP error: " << response->status << '\n';
            std::cerr << response->body << '\n';
            return;
        }

        std::cout << response->body << '\n';
        std::cout << "HTTP status: " << response->status << '\n';
        std::cout << "Response body: " << response->body << '\n';
    }
}


int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    apiClientTest();

    return EXIT_SUCCESS;
}

