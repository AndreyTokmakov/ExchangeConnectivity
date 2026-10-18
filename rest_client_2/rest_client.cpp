/**============================================================================
Name        : rest_client.cpp
Created on  : 16.10.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : rest_client.cpp
============================================================================**/

#include "rest_client.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <algorithm>

namespace
{
    // Lowercase helper for case-insensitive HTTP header handling.
    std::string toLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](const unsigned char c) { return std::tolower(c); });
        return s;
    }

    // Case-insensitive header presence check.
    bool hasHeader(const std::map<std::string, std::string>& headers,
                   const std::string& name)
    {
        const std::string needle = toLower(name);
        return std::any_of(headers.begin(), headers.end(),
                                   [&needle](const auto& kv) { return toLower(kv.first) == needle; });
    }

    // Normalizes a Location header into a path+query string.
    // Throws if the Location points to a different host.
    std::string extractRedirectPath(const std::string& location,
                                    const std::string& expectedHost)
    {
        // Absolute URL: "scheme://host[:port]/path..."
        if (const std::size_t scheme_end = location.find("://");
            scheme_end != std::string::npos)
    {
            const std::string rest = location.substr(scheme_end + 3);
            const std::size_t slash = rest.find('/');
            const std::string hostPort =
                (slash == std::string::npos) ? rest : rest.substr(0, slash);

            // Strip ":port" if present, then compare hosts.
            const std::string locHost = hostPort.substr(0, hostPort.find(':'));
            if (toLower(locHost) != toLower(expectedHost)) {
                throw std::runtime_error(
                    "Cross-host redirect not allowed: " + location);
        }
            return (slash == std::string::npos) ? "/" : rest.substr(slash);
        }

        // Relative path: "/x/y" or "x/y"
        if (!location.empty() && location.front() == '/') {
            return location;
        }
        return "/" + location;
    }
}

namespace networking
{
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace ssl   = boost::asio::ssl;
    using tcp       = boost::asio::ip::tcp;

    std::string HttpResponse::header(const std::string& key) const
    {
        const auto it = headers.find(toLower(key));
        return it != headers.end() ? it->second : std::string{};
    }

    RestClient::RestClient(ClientConfig cfg): config { std::move(cfg) },
                                              sslContext { ssl::context::tls_client }
    {
        if (config.host.empty()) {
            throw std::invalid_argument("RestClient: host is empty");
        }
        if (config.port.empty()) {
            config.port = config.https ? "443" : "80";
        }

        // Host header: append port only when non-standard.
        const bool defaultPort = (config.https && config.port == "443") ||
            (!config.https && config.port == "80");
        hostHeader = defaultPort ? config.host : (config.host + ":" + config.port);

        if (config.verifyPeer)
        {
            sslContext.set_verify_mode(ssl::verify_peer);
            if (!config.ca_cert_path.empty()) {
                sslContext.load_verify_file(config.ca_cert_path);
            } else {
                sslContext.set_default_verify_paths();
            }
        } else {
            sslContext.set_verify_mode(ssl::verify_none);
        }

        if (!config.client_cert_path.empty()) {
            sslContext.use_certificate_chain_file(config.client_cert_path);
        }
        if (!config.client_key_path.empty()) {
            sslContext.use_private_key_file(config.client_key_path, ssl::context::pem);
        }

        // Eager first resolve so misconfiguration fails fast.
        ensureEndpointsFresh();
    }

    HttpResponse RestClient::get(const std::string& target,
                                 const std::map<std::string, std::string>& headers)
    {
        return request(http::verb::get, target, "", headers);
    }

    HttpResponse RestClient::post(const std::string& target,
                                  const std::string& body,
                                  const std::string& content_type,
                                  const std::map<std::string, std::string>& headers)
    {
        std::map<std::string, std::string> hdrs = headers;
        if (!hasHeader(hdrs, "Content-Type")) {
            hdrs["Content-Type"] = content_type;
        }
        return request(http::verb::post, target, body, hdrs);
    }

    HttpResponse RestClient::put(const std::string& target,
                                 const std::string& body,
                                 const std::string& content_type,
                                 const std::map<std::string, std::string>& headers)
    {
        std::map<std::string, std::string> hdrs = headers;
        if (!hasHeader(hdrs, "Content-Type")) {
            hdrs["Content-Type"] = content_type;
        }
        return request(http::verb::put, target, body, hdrs);
    }

    HttpResponse RestClient::del(const std::string& target,
                                 const std::map<std::string, std::string>& headers)
    {
        return request(http::verb::delete_, target, "", headers);
    }

    HttpResponse RestClient::request(const http::verb method,
                                     const std::string& target,
                                     const std::string& body,
                                     const std::map<std::string, std::string>& headers)
    {
        return execute(method, target, body, headers, 0);
    }

    HttpResponse RestClient::toHttpResponse(const http::verb method,
                                            const http::response<http::string_body>& res)
    {
        HttpResponse out;
        out.status  = res.result_int();
        out.reason.assign(res.reason().data(), res.reason().size());
        out.body = res.body();
        out.method = method;
        for (const auto& f : res.base()) {
            out.headers.emplace(toLower(std::string(f.name_string())), std::string(f.value()));
        }
        return out;
    }

    bool RestClient::isRedirectStatus(const uint32_t status) noexcept
    {
        switch (status) {
            case 301:
                case 302:
                case 303:
                case 307:
                case 308:
                return true;
            default:
                return false;
        }
    }

    std::string RestClient::resolveRedirect(const std::string& location) const
    {
        return extractRedirectPath(location, config.host);
    }

    void RestClient::ensureEndpointsFresh()
    {
        const std::chrono::time_point now = std::chrono::steady_clock::now();
        if (const bool expired = endpoints.empty() || (now - endpointsResolvedAt) > config.dnsTtl; !expired) {
            return;
        }
        tcp::resolver resolver(ioContext);
        endpoints = resolver.resolve(config.host, config.port);
        endpointsResolvedAt = now;
    }

    HttpResponse RestClient::execute(const http::verb method,
                                     const std::string& target,
                                     const std::string& body,
                                     const std::map<std::string, std::string>& headers,
                                     const std::size_t redirect_count)
    {
        // Safe for future async usage; harmless in sync mode.
        ioContext.restart();
        ensureEndpointsFresh();

        http::request<http::string_body> req { method, target, 11 };
        req.set(http::field::host, hostHeader);
        req.set(http::field::user_agent, config.user_agent);
        req.set(http::field::accept, "*/*");
        for (const auto& [name, value] : headers) {
            req.set(name, value);
        }

        using enum http::verb;
        if (!body.empty() || method == post || method == put || method == patch) {
            req.body() = body;
            req.prepare_payload();
        }

        beast::flat_buffer buffer;
        HttpResponse out;
        beast::error_code errCode;
        http::response_parser<http::string_body> parser;
        parser.body_limit(config.maxBodyBytes);
        if (config.https)
        {
            beast::ssl_stream<beast::tcp_stream> stream(ioContext, sslContext);
            if (!SSL_set_tlsext_host_name(stream.native_handle(), config.host.c_str())) {
                throw std::runtime_error("SNI setup failed");
            }
            if (config.verifyPeer)
            {   // Hostname verification via OpenSSL (works on any Boost version).
                X509_VERIFY_PARAM* param = SSL_get0_param(stream.native_handle());
                X509_VERIFY_PARAM_set_hostflags( param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
                if (!X509_VERIFY_PARAM_set1_host(param, config.host.c_str(), 0)) {
                    throw std::runtime_error("X509_VERIFY_PARAM_set1_host failed");
                }
            }

            beast::get_lowest_layer(stream).expires_after(config.timeout);
            beast::get_lowest_layer(stream).connect(endpoints);
            stream.handshake(ssl::stream_base::client);

            http::write(stream, req);
            http::read(stream, buffer, parser);

            out = toHttpResponse(method, parser.get());

            // Graceful shutdown; ignore errors (peer may already be closed).
            stream.shutdown(errCode);
        }
        else
        {
            beast::tcp_stream stream(ioContext);
            stream.expires_after(config.timeout);
            stream.connect(endpoints);

            http::write(stream, req);
            http::read(stream, buffer, parser);

            out = toHttpResponse(method, parser.get());
            const auto _ = stream.socket().shutdown(tcp::socket::shutdown_both, errCode);
        }


        // Redirect handling.
        if (config.followRedirects && isRedirectStatus(out.status) && redirect_count < config.maxRedirects)
        {
            if (const std::string loc = out.header("Location"); !loc.empty())
            {
                // 303 See Other forces GET; 307/308 preserve method and body.
                const http::verb new_method = (out.status == 303) ? get : method;
                const std::string new_body = (out.status == 303) ? std::string{} : body;

                // May throw on cross-host redirect; the caller will see the
                // exception rather than a silently truncated target.
                const std::string new_target = resolveRedirect(loc);
                return execute(new_method, new_target, new_body, headers, redirect_count + 1);
            }
        }
        return out;
    }
}