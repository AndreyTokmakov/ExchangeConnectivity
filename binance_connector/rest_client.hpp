/**============================================================================
Name        : rest_client.hpp
Created on  : 20.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : rest_client.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_BINANCE_REST_CLIENT_HPP
#define EXCHANGECONNECTIVITY_BINANCE_REST_CLIENT_HPP

#include "types.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <string>

namespace binance
{
    class SnapshotRequest;

    class RestClient
    {
    public:
        using Response = std::expected<std::string, RestFailure>;
        using ResponseHandler = std::function<void(Response)>;

        RestClient(boost::asio::io_context& ioContext,
                   boost::asio::ssl::context& sslContext);
        ~RestClient();

        RestClient(const RestClient&) = delete;
        RestClient& operator=(const RestClient&) = delete;

        void asyncGetDepthSnapshot(std::string symbol, ResponseHandler handler);

    private:
        boost::asio::io_context& ioContext;
        boost::asio::ssl::context& sslContext;
    };
}

#endif //EXCHANGECONNECTIVITY_BINANCE_REST_CLIENT_HPP
