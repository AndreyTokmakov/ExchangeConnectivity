/**============================================================================
Name        : rest_client.hpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : rest_client.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_REST_CLIENT_HPP
#define EXCHANGECONNECTIVITY_REST_CLIENT_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http/verb.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>


namespace networking
{
    enum class RestError : std::uint8_t
    {
        ResolveFailed,
        ConnectFailed,
        TlsHandshakeFailed,
        WriteFailed,
        ReadFailed,
        Timeout,
        InvalidRequest,
        HttpStatus,
        InvalidResponse
    };

    struct RestFailure
    {
        RestError error { RestError::InvalidRequest };
        std::string message;
    };

    struct HttpRequest
    {
        std::string host;
        std::string port { "443" };
        std::string target { "/" };
        boost::beast::http::verb method { boost::beast::http::verb::get };
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;
    };

    struct HttpResponse
    {
        std::uint32_t status { 0 };
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;
    };

    class RestSession;

    class RestClient
    {
    public:
        using Response = std::expected<HttpResponse, RestFailure>;
        using ResponseHandler = std::function<void(Response)>;

        RestClient(boost::asio::io_context& ioContext, boost::asio::ssl::context& sslContext);
        ~RestClient();

        RestClient(const RestClient&) = delete;
        RestClient& operator=(const RestClient&) = delete;

        void asyncRequest(HttpRequest& request, ResponseHandler handler) const;

    private:
        boost::asio::io_context& ioContext;
        boost::asio::ssl::context& sslContext;
    };
}

#endif //EXCHANGECONNECTIVITY_REST_CLIENT_HPP
