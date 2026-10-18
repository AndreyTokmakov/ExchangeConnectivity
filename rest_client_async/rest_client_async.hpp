/**============================================================================
Name        : rest_client_async.hpp
Created on  : 12.09.2026
Author      : Andrei Tokmakov
Version     : 3.0
Copyright   : Your copyright notice
Description : Async REST client (HTTP/HTTPS) on Boost.Beast.
              Callback-based (no coroutines, no templates), connection pool,
              std::expected error handling.
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_REST_CLIENT_ASYNC_HPP
#define EXCHANGECONNECTIVITY_REST_CLIENT_ASYNC_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>

namespace networking
{
    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;

    // ---------------- errors ----------------
    enum class RestError
    {
        ResolveFailed,
        ConnectFailed,
        TlsHandshakeFailed,
        WriteFailed,
        ReadFailed,
        BodyLimitExceeded,
        ShutdownFailed,
        TooManyRedirects,
        CrossHostRedirect,
        PoolExhausted,
    };

    [[nodiscard]] const char* toString(RestError err) noexcept;

    // ---------------- response ----------------
    struct HttpResponse
    {
        uint32_t status { 0 };
        std::string reason;
        std::string body;
        std::map<std::string, std::string> headers;   // keys are lowercased
        http::verb method { http::verb::get };

        [[nodiscard]] bool ok() const { return status >= 200 && status < 300; }

        [[nodiscard]]
        std::string header(const std::string& key) const;
    };

    using RestResult  = std::expected<HttpResponse, RestError>;
    using RestHandler = std::function<void(RestResult)>;

    // ---------------- config ----------------
    struct ClientConfig
    {
        std::string host;
        std::string port;
        std::chrono::seconds timeout { 30 };
        std::chrono::seconds dnsTtl { 60 };

        std::string ca_cert_path;
        std::string client_cert_path;
        std::string client_key_path;
        std::string user_agent = "RestClientAsync/3.0";
        bool        https = true;
        bool        verifyPeer = true;
        bool        followRedirects = true;
        std::size_t maxRedirects = 5;
        std::size_t maxBodyBytes = 8 * 1024 * 1024;

        std::size_t maxPoolSize = 8;
        bool        keepAlive = true;
    };

    // ---------------- client ----------------
    class RestClientAsync
    {
    public:
        RestClientAsync(asio::io_context& ioContext, ClientConfig cfg);
        ~RestClientAsync();

        RestClientAsync(const RestClientAsync&) = delete;
        RestClientAsync& operator=(const RestClientAsync&) = delete;

        // ---- callback-style API ----

        void get(const std::string& target,
                 const std::map<std::string, std::string>& headers,
                 RestHandler handler);

        void get(const std::string& target, RestHandler handler);

        void post(const std::string& target,
                  const std::string& body,
                  const std::string& content_type,
                  const std::map<std::string, std::string>& headers,
                  RestHandler handler);

        void post(const std::string& target,
                  const std::string& body,
                  RestHandler handler);

        void put(const std::string& target,
                 const std::string& body,
                 const std::string& content_type,
                 const std::map<std::string, std::string>& headers,
                 RestHandler handler);

        void put(const std::string& target,
                 const std::string& body,
                 RestHandler handler);

        void del(const std::string& target,
                 const std::map<std::string, std::string>& headers,
                 RestHandler handler);

        void del(const std::string& target, RestHandler handler);

        void request(http::verb method,
                     const std::string& target,
                     const std::string& body,
                     const std::map<std::string, std::string>& headers,
                     RestHandler handler);

        // ---- future-style API ----

        std::future<RestResult> getAsync(
            const std::string& target,
            const std::map<std::string, std::string>& headers = {});

        std::future<RestResult> postAsync(
            const std::string& target,
            const std::string& body,
            const std::string& content_type = "application/json",
            const std::map<std::string, std::string>& headers = {});

        std::future<RestResult> putAsync(
            const std::string& target,
            const std::string& body,
            const std::string& content_type = "application/json",
            const std::map<std::string, std::string>& headers = {});

        std::future<RestResult> delAsync(
            const std::string& target,
            const std::map<std::string, std::string>& headers = {});

    private:
        class Connection;
        struct RequestState;

        void startRequest(http::verb method,
                          const std::string& target,
                          const std::string& body,
                          const std::map<std::string, std::string>& headers,
                          std::size_t redirectCount,
                          RestHandler handler);

        void doConnect(std::shared_ptr<RequestState> st,
                       asio::ip::tcp::resolver::results_type endpoints);

        void sendAndReceive(std::shared_ptr<RequestState> st);

        void onResponse(std::shared_ptr<RequestState> st,
                        const boost::system::error_code& ec);

        void releaseConnection(std::shared_ptr<Connection> conn, bool reusable);

        static HttpResponse toHttpResponse(
            http::verb method,
            const http::response<http::string_body>& res);

        static bool isRedirectStatus(uint32_t status) noexcept;

        std::expected<std::string, RestError>
        resolveRedirect(const std::string& location) const;

        ClientConfig config;
        asio::io_context& ioContext;
        asio::ssl::context sslContext;
        std::string hostHeader;

        std::deque<std::shared_ptr<Connection>> idlePool;
    };

} // namespace networking
#endif //EXCHANGECONNECTIVITY_REST_CLIENT_ASYNC_HPP