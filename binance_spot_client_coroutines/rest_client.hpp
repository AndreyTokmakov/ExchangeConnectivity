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

#include <boost/asio/async_result.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/beast/http.hpp>

namespace trading::networking
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

        RestClient(boost::asio::io_context& ioContext,
                   boost::asio::ssl::context& sslContext);

        ~RestClient();

        RestClient(const RestClient&) = delete;
        RestClient& operator=(const RestClient&) = delete;

        template <typename CompletionToken>
        auto asyncRequest(HttpRequest request, CompletionToken&& token) const
        {
            namespace asio = boost::asio;

            return asio::async_initiate<CompletionToken, void(Response)>([this, request = std::move(request)](auto handler) mutable
            {
                using Handler = std::decay_t<decltype(handler)>;
                std::shared_ptr<Handler> sharedHandler = std::make_shared<Handler>(std::move(handler));
                auto executor = asio::get_associated_executor(*sharedHandler, ioContext);

                ResponseHandler callback = [sharedHandler, executor](Response response) mutable{
                    asio::dispatch(executor,[sharedHandler, response = std::move(response)]() mutable {
                        (*sharedHandler)(std::move(response));
                    });
                };

                startRequest(std::move(request), std::move(callback));
            },token);
        }

    private:
        void startRequest(HttpRequest request, ResponseHandler handler) const;

        boost::asio::io_context& ioContext;
        boost::asio::ssl::context& sslContext;
    };
}

#endif //EXCHANGECONNECTIVITY_REST_CLIENT_HPP
