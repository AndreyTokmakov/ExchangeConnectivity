/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/

#include <iostream>
#include <string_view>
#include <vector>

#include "rest_client.hpp"


int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    using networking::HttpResponse;


    const std::vector<std::string_view> args(argv + 1, argv + argc);
    try {
        networking::ClientConfig cfg;
        cfg.host   = "httpbin.org";
        cfg.port   = "443";
        cfg.https  = true;
        cfg.timeout = std::chrono::seconds(15);

        networking::RestClient client(cfg);

        HttpResponse getResponse = client.get("/get");
        std::cout << "GET " << getResponse.body << ' ' << getResponse.reason << '\n';

        // POST JSON
        HttpResponse postResponse = client.post("/post", R"({"name":"Alice","age":30})");
        std::cout << "POST " << postResponse.body << '\n';

        // GET с query-string (передаётся как часть target)
        HttpResponse queryResponse = client.get("/get?foo=bar&baz=1");
        std::cout << "QUERY " << queryResponse.body << '\n';

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return EXIT_SUCCESS;
}

