/**============================================================================
Name        : rest_client_async.cpp
Created on  : 16.10.2026
Author      : Andrei Tokmakov
Version     : 3.0
Copyright   : Your copyright notice
Description : Async REST client (HTTP/HTTPS) on Boost.Beast.
              Callback-based, connection pool, std::expected.
============================================================================**/

#include "rest_client_async.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <variant>

namespace networking
{
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace ssl   = boost::asio::ssl;
    using tcp       = boost::asio::ip::tcp;

    namespace
    {
        std::string toLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](const unsigned char c) { return std::tolower(c); });
            return s;
        }

        bool hasContentTypeHeader(const std::map<std::string, std::string>& headers)
        {
            return std::any_of(headers.begin(), headers.end(),
                [](const auto& kv) { return toLower(kv.first) == "content-type"; });
        }

        std::string extractRedirectPath(const std::string& location,
                                        const std::string& expectedHost)
        {
            if (const std::size_t scheme_end = location.find("://");
                scheme_end != std::string::npos)
            {
                const std::string rest = location.substr(scheme_end + 3);
                const std::size_t slash = rest.find('/');
                const std::string hostPort =
                    (slash == std::string::npos) ? rest : rest.substr(0, slash);

                const std::string locHost = hostPort.substr(0, hostPort.find(':'));
                if (toLower(locHost) != toLower(expectedHost)) {
                    throw std::runtime_error(
                        "Cross-host redirect not allowed: " + location);
                }
                return (slash == std::string::npos) ? "/" : rest.substr(slash);
            }
            if (!location.empty() && location.front() == '/') {
                return location;
            }
            return "/" + location;
        }
    }

    // ---------------- RestError ----------------
    const char* toString(const RestError err) noexcept
    {
        switch (err) {
            case RestError::ResolveFailed:       return "ResolveFailed";
            case RestError::ConnectFailed:       return "ConnectFailed";
            case RestError::TlsHandshakeFailed:  return "TlsHandshakeFailed";
            case RestError::WriteFailed:         return "WriteFailed";
            case RestError::ReadFailed:          return "ReadFailed";
            case RestError::BodyLimitExceeded:   return "BodyLimitExceeded";
            case RestError::ShutdownFailed:      return "ShutdownFailed";
            case RestError::TooManyRedirects:    return "TooManyRedirects";
            case RestError::CrossHostRedirect:   return "CrossHostRedirect";
            case RestError::PoolExhausted:       return "PoolExhausted";
        }
        return "Unknown";
    }

    // ---------------- HttpResponse ----------------
    std::string HttpResponse::header(const std::string& key) const
    {
        const auto it = headers.find(toLower(key));
        return it != headers.end() ? it->second : std::string{};
    }

    // ---------------- Connection ----------------
    class RestClientAsync::Connection
    {
    public:
        using TcpStream = beast::tcp_stream;
        using TlsStream = beast::ssl_stream<beast::tcp_stream>;

        explicit Connection(TcpStream&& s): stream(std::move(s)), isTls(false) {}
        explicit Connection(TlsStream&& s): stream(std::move(s)), isTls(true) {}

        [[nodiscard]] bool tls() const noexcept { return isTls; }
        [[nodiscard]] beast::flat_buffer& buffer() noexcept { return readBuffer; }
        TcpStream& tcpStream() { return std::get<TcpStream>(stream); }
        TlsStream& tlsStream() { return std::get<TlsStream>(stream); }

    private:
        std::variant<TcpStream, TlsStream> stream;
        bool isTls;
        beast::flat_buffer readBuffer;
    };

    // ---------------- RequestState ----------------
    struct RestClientAsync::RequestState
    {
        http::verb method;
        std::string target;
        std::string body;
        std::map<std::string, std::string> headers;
        std::size_t redirectCount;

        std::shared_ptr<Connection> conn;
        http::request<http::string_body> req;
        http::response_parser<http::string_body> parser;

        RestHandler handler;
    };

    // ---------------- ctor / dtor ----------------
    RestClientAsync::RestClientAsync(asio::io_context& ioCtx, ClientConfig cfg):
        config { std::move(cfg) },
        ioContext { ioCtx },
        sslContext { ssl::context::tls_client }
    {
        if (config.host.empty()) {
            throw std::invalid_argument("RestClientAsync: host is empty");
        }
        if (config.port.empty()) {
            config.port = config.https ? "443" : "80";
        }

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
    }

    RestClientAsync::~RestClientAsync()
    {
        for (auto& conn : idlePool)
        {
            if (conn->tls())
            {
                beast::error_code ec;
                conn->tlsStream().shutdown(ec);
            }
            else
            {
                beast::error_code ec;
                conn->tcpStream().socket().shutdown(tcp::socket::shutdown_both, ec);
            }
        }
        idlePool.clear();
    }

    // ---------------- pool ----------------
    void RestClientAsync::releaseConnection(std::shared_ptr<Connection> conn,
                                            const bool reusable)
    {
        if (!conn) {
            return;
        }

        if (!reusable || !config.keepAlive || idlePool.size() >= config.maxPoolSize)
        {
            if (conn->tls())
            {
                beast::error_code ec;
                conn->tlsStream().shutdown(ec);
            }
            else
            {
                beast::error_code ec;
                conn->tcpStream().socket().shutdown(tcp::socket::shutdown_both, ec);
            }
            return;
        }

        conn->buffer().consume(conn->buffer().size());
        if (conn->tls()) {
            beast::get_lowest_layer(conn->tlsStream()).expires_after(config.timeout);
        } else {
            conn->tcpStream().expires_after(config.timeout);
        }
        idlePool.push_back(std::move(conn));
    }

    // ---------------- helpers ----------------
    HttpResponse RestClientAsync::toHttpResponse(
        const http::verb method,
        const http::response<http::string_body>& res)
    {
        HttpResponse out;
        out.status = res.result_int();
        out.reason.assign(res.reason().data(), res.reason().size());
        out.body   = res.body();
        out.method = method;
        for (const auto& f : res.base()) {
            out.headers.emplace(toLower(std::string(f.name_string())),
                                std::string(f.value()));
        }
        return out;
    }

    bool RestClientAsync::isRedirectStatus(const uint32_t status) noexcept
    {
        switch (status) {
            case 301: case 302: case 303: case 307: case 308:
                return true;
            default:
                return false;
        }
    }

    std::expected<std::string, RestError>
    RestClientAsync::resolveRedirect(const std::string& location) const
    {
        try {
            return extractRedirectPath(location, config.host);
        } catch (const std::exception&) {
            return std::unexpected{ RestError::CrossHostRedirect };
        }
    }

    // ---------------- callback-style API ----------------
    void RestClientAsync::request(const http::verb method,
                                  const std::string& target,
                                  const std::string& body,
                                  const std::map<std::string, std::string>& headers,
                                  RestHandler handler)
    {
        startRequest(method, target, body, headers, 0, std::move(handler));
    }

    void RestClientAsync::get(const std::string& target,
                              const std::map<std::string, std::string>& headers,
                              RestHandler handler)
    {
        request(http::verb::get, target, "", headers, std::move(handler));
    }

    void RestClientAsync::get(const std::string& target, RestHandler handler)
    {
        request(http::verb::get, target, "", {}, std::move(handler));
    }

    void RestClientAsync::post(const std::string& target,
                               const std::string& body,
                               const std::string& content_type,
                               const std::map<std::string, std::string>& headers,
                               RestHandler handler)
    {
        auto h = headers;
        if (!hasContentTypeHeader(h)) {
            h["Content-Type"] = content_type;
        }
        request(http::verb::post, target, body, h, std::move(handler));
    }

    void RestClientAsync::post(const std::string& target,
                               const std::string& body,
                               RestHandler handler)
    {
        post(target, body, "application/json", {}, std::move(handler));
    }

    void RestClientAsync::put(const std::string& target,
                              const std::string& body,
                              const std::string& content_type,
                              const std::map<std::string, std::string>& headers,
                              RestHandler handler)
    {
        auto h = headers;
        if (!hasContentTypeHeader(h)) {
            h["Content-Type"] = content_type;
        }
        request(http::verb::put, target, body, h, std::move(handler));
    }

    void RestClientAsync::put(const std::string& target,
                              const std::string& body,
                              RestHandler handler)
    {
        put(target, body, "application/json", {}, std::move(handler));
    }

    void RestClientAsync::del(const std::string& target,
                              const std::map<std::string, std::string>& headers,
                              RestHandler handler)
    {
        request(http::verb::delete_, target, "", headers, std::move(handler));
    }

    void RestClientAsync::del(const std::string& target, RestHandler handler)
    {
        request(http::verb::delete_, target, "", {}, std::move(handler));
    }

    // ---------------- future-style API ----------------
    namespace
    {
        std::future<RestResult> makeFuture()
        {
            std::promise<RestResult> promise;
            auto future = promise.get_future();
            // Return a pair: the future and a callable to fulfill the promise.
            // We do this via a shared_ptr to keep the promise alive.
            struct Holder
            {
                std::promise<RestResult> promise;
                std::future<RestResult>  future;
            };
            // Simpler: use a shared_ptr<promise> captured by the handler.
            // See overloads below.
            (void)promise;
            (void)future;
            throw std::logic_error("unused");
        }
    }

    std::future<RestResult> RestClientAsync::getAsync(
        const std::string& target,
        const std::map<std::string, std::string>& headers)
    {
        auto promise = std::make_shared<std::promise<RestResult>>();
        auto future  = promise->get_future();
        startRequest(http::verb::get, target, "", headers, 0,
            [promise](RestResult result) {
                promise->set_value(std::move(result));
            });
        return future;
    }

    std::future<RestResult> RestClientAsync::postAsync(
        const std::string& target,
        const std::string& body,
        const std::string& content_type,
        const std::map<std::string, std::string>& headers)
    {
        auto promise = std::make_shared<std::promise<RestResult>>();
        auto future  = promise->get_future();
        auto h = headers;
        if (!hasContentTypeHeader(h)) {
            h["Content-Type"] = content_type;
        }
        startRequest(http::verb::post, target, body, h, 0,
            [promise](RestResult result) {
                promise->set_value(std::move(result));
            });
        return future;
    }

    std::future<RestResult> RestClientAsync::putAsync(
        const std::string& target,
        const std::string& body,
        const std::string& content_type,
        const std::map<std::string, std::string>& headers)
    {
        auto promise = std::make_shared<std::promise<RestResult>>();
        auto future  = promise->get_future();
        auto h = headers;
        if (!hasContentTypeHeader(h)) {
            h["Content-Type"] = content_type;
        }
        startRequest(http::verb::put, target, body, h, 0,
            [promise](RestResult result) {
                promise->set_value(std::move(result));
            });
        return future;
    }

    std::future<RestResult> RestClientAsync::delAsync(
        const std::string& target,
        const std::map<std::string, std::string>& headers)
    {
        auto promise = std::make_shared<std::promise<RestResult>>();
        auto future  = promise->get_future();
        startRequest(http::verb::delete_, target, "", headers, 0,
            [promise](RestResult result) {
                promise->set_value(std::move(result));
            });
        return future;
    }

    // ---------------- engine ----------------
    void RestClientAsync::startRequest(
        const http::verb method,
        const std::string& target,
        const std::string& body,
        const std::map<std::string, std::string>& headers,
        const std::size_t redirectCount,
        RestHandler handler)
    {
        auto st = std::make_shared<RequestState>();
        st->method        = method;
        st->target        = target;
        st->body          = body;
        st->headers       = headers;
        st->redirectCount = redirectCount;
        st->handler       = std::move(handler);

        st->req = http::request<http::string_body>{ method, target, 11 };
        st->req.set(http::field::host, hostHeader);
        st->req.set(http::field::user_agent, config.user_agent);
        st->req.set(http::field::accept, "*/*");
        for (const auto& [name, value] : headers) {
            st->req.set(name, value);
        }

        using enum http::verb;
        if (!body.empty() || method == post || method == put || method == patch) {
            st->req.body() = body;
            st->req.prepare_payload();
        }
        st->parser.body_limit(config.maxBodyBytes);

        // Fast path: idle connection available.
        if (!idlePool.empty())
        {
            st->conn = std::move(idlePool.front());
            idlePool.pop_front();
            sendAndReceive(st);
            return;
        }

        // Slow path: resolve → connect → (handshake) → send.
        auto resolver = std::make_shared<tcp::resolver>(ioContext);
        auto* self = this;

        resolver->async_resolve(config.host, config.port,
            [self, st, resolver](const boost::system::error_code& ec,
                                 tcp::resolver::results_type endpoints)
            {
                if (ec) {
                    st->handler(std::unexpected{ RestError::ResolveFailed });
                    return;
                }
                self->doConnect(st, std::move(endpoints));
            });
    }

    void RestClientAsync::doConnect(std::shared_ptr<RequestState> st,
                                    tcp::resolver::results_type endpoints)
    {
        auto* self = this;

        if (config.https)
        {
            auto stream = std::make_shared<Connection::TlsStream>(ioContext, sslContext);

            if (!SSL_set_tlsext_host_name(stream->native_handle(),
                                          config.host.c_str())) {
                st->handler(std::unexpected{ RestError::TlsHandshakeFailed });
                return;
            }
            if (config.verifyPeer)
            {
                X509_VERIFY_PARAM* param = SSL_get0_param(stream->native_handle());
                X509_VERIFY_PARAM_set_hostflags(param,
                    X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
                if (!X509_VERIFY_PARAM_set1_host(param, config.host.c_str(), 0)) {
                    st->handler(std::unexpected{ RestError::TlsHandshakeFailed });
                    return;
                }
            }

            beast::get_lowest_layer(*stream).expires_after(config.timeout);
            beast::get_lowest_layer(*stream).async_connect(endpoints,
                [self, st, stream](const boost::system::error_code& ec,
                                   const tcp::endpoint&)
                {
                    if (ec) {
                        st->handler(std::unexpected{ RestError::ConnectFailed });
                        return;
                    }
                    stream->async_handshake(ssl::stream_base::client,
                        [self, st, stream](const boost::system::error_code& ec2)
                        {
                            if (ec2) {
                                st->handler(
                                    std::unexpected{ RestError::TlsHandshakeFailed });
                                return;
                            }
                            beast::get_lowest_layer(*stream)
                                .expires_after(self->config.timeout);
                            st->conn = std::make_shared<Connection>(std::move(*stream));
                            self->sendAndReceive(st);
                        });
                });
        }
        else
        {
            auto stream = std::make_shared<Connection::TcpStream>(ioContext);
            stream->expires_after(config.timeout);
            stream->async_connect(endpoints,
                [self, st, stream](const boost::system::error_code& ec,
                                   const tcp::endpoint&)
                {
                    if (ec) {
                        st->handler(std::unexpected{ RestError::ConnectFailed });
                        return;
                    }
                    st->conn = std::make_shared<Connection>(std::move(*stream));
                    self->sendAndReceive(st);
                });
        }
    }

    void RestClientAsync::sendAndReceive(std::shared_ptr<RequestState> st)
    {
        auto* self = this;

        if (st->conn->tls())
        {
            auto& stream = st->conn->tlsStream();
            beast::get_lowest_layer(stream).expires_after(config.timeout);
            http::async_write(stream, st->req,
                [self, st](const boost::system::error_code& ec, std::size_t)
                {
                    if (ec) {
                        self->releaseConnection(std::move(st->conn), false);
                        st->handler(std::unexpected{ RestError::WriteFailed });
                        return;
                    }
                    auto& s = st->conn->tlsStream();
                    http::async_read(s, st->conn->buffer(), st->parser,
                        [self, st](const boost::system::error_code& ec2, std::size_t)
                        {
                            self->onResponse(st, ec2);
                        });
                });
        }
        else
        {
            auto& stream = st->conn->tcpStream();
            stream.expires_after(config.timeout);
            http::async_write(stream, st->req,
                [self, st](const boost::system::error_code& ec, std::size_t)
                {
                    if (ec) {
                        self->releaseConnection(std::move(st->conn), false);
                        st->handler(std::unexpected{ RestError::WriteFailed });
                        return;
                    }
                    auto& s = st->conn->tcpStream();
                    http::async_read(s, st->conn->buffer(), st->parser,
                        [self, st](const boost::system::error_code& ec2, std::size_t)
                        {
                            self->onResponse(st, ec2);
                        });
                });
        }
    }

    void RestClientAsync::onResponse(std::shared_ptr<RequestState> st,
                                     const boost::system::error_code& ec)
    {
        if (ec)
        {
            const auto err = (ec == http::error::body_limit)
                ? RestError::BodyLimitExceeded
                : RestError::ReadFailed;
            releaseConnection(std::move(st->conn), false);
            st->handler(std::unexpected{ err });
            return;
        }

        HttpResponse out = toHttpResponse(st->method, st->parser.get());
        const bool reusable = st->parser.get().keep_alive();

        // Redirect?
        if (config.followRedirects && isRedirectStatus(out.status) &&
            st->redirectCount < config.maxRedirects)
        {
            const std::string loc = out.header("Location");
            if (!loc.empty())
            {
                auto newTarget = resolveRedirect(loc);
                if (!newTarget) {
                    releaseConnection(std::move(st->conn), reusable);
                    st->handler(std::unexpected{ newTarget.error() });
                    return;
                }

                const auto newMethod =
                    (out.status == 303) ? http::verb::get : st->method;
                const std::string newBody =
                    (out.status == 303) ? std::string{} : st->body;

                releaseConnection(std::move(st->conn), reusable);

                // Move handler out (it must survive the state reset).
                auto handler = std::move(st->handler);
                startRequest(newMethod, *newTarget, newBody, st->headers,
                             st->redirectCount + 1, std::move(handler));
                return;
            }
        }

        releaseConnection(std::move(st->conn), reusable);
        st->handler(out);
    }

} // namespace networking