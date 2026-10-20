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
#include <boost/beast/http.hpp>

#include <expected>
#include <map>
#include <string>

namespace networking
{
    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;

    struct HttpResponse
    {
        uint32_t status { 0 };
        std::string reason;
        std::string body;
        std::map<std::string, std::string> headers;
        boost::beast::http::verb method { boost::beast::http::verb::get };

        [[nodiscard]]
        bool ok() const {
            return status >= 200 && status < 300;
        }

        // Case-insensitive header lookup (HTTP header names are case-insensitive)
        [[nodiscard]]
        std::string header(const std::string& key) const;
    };

    struct ClientConfig
    {
        std::string host;                        // "api.example.com"
        std::string port;                        // "443" / "80"
        std::chrono::seconds timeout { 30 };
        std::chrono::seconds dnsTtl { 60 };      // re-resolve after this interval

        std::string ca_cert_path;
        std::string client_cert_path;            // mTLS
        std::string client_key_path;             // mTLS
        std::string user_agent = "RestClient/1.0";
        bool        https = true;
        bool        verifyPeer = true;
        bool        followRedirects = true;
        std::size_t maxRedirects = 5;
        std::size_t maxBodyBytes = 8 * 1024 * 1024;  // safety limit on response body
    };

    class RestClient
    {
    public:
        explicit RestClient(ClientConfig cfg);

        [[nodiscard]]
        HttpResponse get(const std::string& target,
                         const std::map<std::string, std::string>& headers = {});

        [[nodiscard]]
        HttpResponse post(const std::string& target,
                          const std::string& body,
                          const std::string& content_type = "application/json",
                          const std::map<std::string, std::string>& headers = {});

        [[nodiscard]]
        HttpResponse put(const std::string& target,
                         const std::string& body,
                         const std::string& content_type = "application/json",
                         const std::map<std::string, std::string>& headers = {});

        [[nodiscard]]
        HttpResponse del(const std::string& target,
                         const std::map<std::string, std::string>& headers = {});

        [[nodiscard]]
        HttpResponse request(http::verb method,
                             const std::string& target,
                             const std::string& body = "",
                             const std::map<std::string, std::string>& headers = {});
    private:

        [[nodiscard]]
        HttpResponse execute(http::verb method,
                             const std::string& target,
                             const std::string& body,
                             const std::map<std::string, std::string>& headers,
                             std::size_t redirect_count);

        static HttpResponse toHttpResponse(http::verb method,
                                           const http::response<http::string_body>& res);

        [[nodiscard]]
        static bool isRedirectStatus(uint32_t status) noexcept;

        // Returns a normalized redirect target or throws on cross-host redirect.
        [[nodiscard]]
        std::string resolveRedirect(const std::string& location) const;

        // Re-resolves endpoints if the cached entry has expired.
        void ensureEndpointsFresh();

        ClientConfig config;
        asio::io_context ioContext;
        asio::ssl::context sslContext;

        asio::ip::tcp::resolver::results_type endpoints{};
        std::chrono::steady_clock::time_point endpointsResolvedAt {};

        std::string hostHeader;
    };
}
#endif //EXCHANGECONNECTIVITY_REST_CLIENT_HPP
