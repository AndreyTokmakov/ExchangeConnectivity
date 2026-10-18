/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/

#include "rest_client_async.hpp"
#include <iostream>


int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    boost::asio::io_context ioContext;

    networking::ClientConfig cfg;
    cfg.host  = "httpbin.org";
    cfg.port  = "443";
    cfg.https = true;

    networking::RestClientAsync client(ioContext, cfg);

    // --- future-style (default) ---
    auto future = client.getAsync("/get");

    std::thread runner([&]{ ioContext.run(); });

    auto result = future.get();
    if (result) {
        std::cout << "GET " << result->status << ' ' << result->reason << '\n';
    } else {
        std::cerr << "GET failed: " << networking::toString(result.error()) << '\n';
    }

    // --- callback-style ---
    client.post("/post", R"({"k":"v"})",
        [](networking::RestResult r) {
            if (r) std::cout << "POST " << r->status << '\n';
            else   std::cerr << networking::toString(r.error()) << '\n';
    });

    runner.join();


    return EXIT_SUCCESS;
}

