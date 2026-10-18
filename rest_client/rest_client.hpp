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
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace networking
{
    namespace asio = boost::asio;

    enum class HttpMethod
    {
        Get,
        Post,
        Put,
        Delete
    };

    enum class RestError
    {
        InvalidMethod,
        ResolveFailed,
        ConnectFailed,
        TlsServerNameFailed,
        TlsHandshakeFailed,
        WriteFailed,
        ReadFailed
    };

    struct RestFailure
    {
        RestError error { RestError::InvalidMethod };
        boost::system::error_code systemError {};
        uint64_t sslError { 0 };
    };

    struct HttpResponse
    {
        unsigned int status { 0 };
        std::string body;
    };

    using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

    class RestClient
    {
    public:
        static constexpr std::string_view UserAgent = "hft-trading-client/1.0";

        explicit RestClient(std::string host, uint16_t port = 443);

        [[nodiscard]]
        std::expected<HttpResponse, RestFailure>
        request(HttpMethod method,
                std::string_view target,
                const HttpHeaders& headers = {},
                std::string_view body = {});

    private:
        std::string host;
        std::string port { 0 };

        asio::io_context ioContext;
        asio::ssl::context sslContext;
        asio::ip::tcp::resolver resolver;
    };
}
#endif //EXCHANGECONNECTIVITY_REST_CLIENT_HPP
