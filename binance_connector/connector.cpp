
#include "connector.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace binance
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    namespace ssl = net::ssl;
    namespace json = boost::json;

    using Tcp = net::ip::tcp;
    using SslStream = beast::ssl_stream<beast::tcp_stream>;

    constexpr std::string_view BinanceHost = "stream.binance.com";
    constexpr std::string_view BinanceRestHost = "api.binance.com";
    constexpr std::string_view HttpsPort = "443";

    std::string toLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char character)
                       {
                           return static_cast<char>(std::tolower(character));
                       });

        return value;
    }

    std::string toUpper(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char character)
                       {
                           return static_cast<char>(std::toupper(character));
                       });

        return value;
    }

    std::uint64_t jsonUnsigned(const json::value& value)
    {
        if (value.is_uint64()) {
            return value.as_uint64();
        }

        if (value.is_int64() && value.as_int64() >= 0) {
            return static_cast<std::uint64_t>(value.as_int64());
        }

        throw std::runtime_error("Expected a non-negative JSON integer");
    }

    std::string jsonString(const json::value& value)
    {
        if (!value.is_string()) {
            throw std::runtime_error("Expected a JSON string");
        }

        return std::string(value.as_string());
    }

    binance::Price parseDecimal(std::string_view value)
    {
        if (value.empty()) {
            throw std::runtime_error("Empty decimal value");
        }

        bool negative = false;
        std::size_t position = 0;

        if (value[position] == '-' || value[position] == '+') {
            negative = value[position] == '-';
            ++position;
        }

        if (position == value.size()) {
            throw std::runtime_error("Invalid decimal value");
        }

        const std::uint64_t MaxValue =
            static_cast<std::uint64_t>(std::numeric_limits<binance::Price>::max());

        const std::uint64_t limit = negative ? MaxValue + 1 : MaxValue;

        std::uint64_t integerPart = 0;
        std::uint64_t fractionalPart = 0;
        std::uint64_t fractionalScale = 1;
        bool hasIntegerDigit = false;
        bool hasFractionalDigit = false;

        for (; position < value.size() && value[position] != '.'; ++position) {
            const char character = value[position];

            if (character < '0' || character > '9') {
                throw std::runtime_error("Invalid decimal digit");
            }

            hasIntegerDigit = true;
            const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');

            if (integerPart > (limit / 10) ||
                (integerPart == (limit / 10) && digit > limit % 10)) {
                throw std::runtime_error("Decimal value overflow");
            }

            integerPart = integerPart * 10 + digit;
        }

        if (position < value.size() && value[position] == '.') {
            ++position;

            for (; position < value.size(); ++position) {
                const char character = value[position];

                if (character < '0' || character > '9') {
                    throw std::runtime_error("Invalid fractional digit");
                }

                hasFractionalDigit = true;

                if (fractionalScale < binance::DecimalScale) {
                    fractionalPart = fractionalPart * 10 +
                        static_cast<std::uint64_t>(character - '0');
                    fractionalScale *= 10;
                } else if (character != '0') {
                    throw std::runtime_error("Decimal precision exceeds 8 places");
                }
            }
        }

        if (!hasIntegerDigit && !hasFractionalDigit) {
            throw std::runtime_error("Invalid decimal value");
        }

        while (fractionalScale < binance::DecimalScale) {
            fractionalPart *= 10;
            fractionalScale *= 10;
        }

        if (integerPart > limit / static_cast<std::uint64_t>(binance::DecimalScale)) {
            throw std::runtime_error("Scaled decimal value overflow");
        }

        const std::uint64_t scaledInteger =
            integerPart * static_cast<std::uint64_t>(binance::DecimalScale);

        if (fractionalPart > limit - scaledInteger) {
            throw std::runtime_error("Scaled decimal value overflow");
        }

        const std::uint64_t result = scaledInteger + fractionalPart;

        if (negative) {
            if (result == MaxValue + 1) {
                return std::numeric_limits<binance::Price>::min();
            }

            return -static_cast<binance::Price>(result);
        }

        return static_cast<binance::Price>(result);
    }

    binance::BookLevel parseBookLevel(const json::value& value)
    {
        if (!value.is_array() || value.as_array().size() < 2) {
            throw std::runtime_error("Invalid order book level");
        }

        const json::array& level = value.as_array();

        return {
            parseDecimal(jsonString(level[0])),
            parseDecimal(jsonString(level[1]))
        };
    }

    std::vector<binance::BookLevel> parseLevels(const json::value& value)
    {
        if (!value.is_array()) {
            throw std::runtime_error("Expected an array of order book levels");
        }

        std::vector<binance::BookLevel> levels;
        const json::array& array = value.as_array();
        levels.reserve(array.size());

        for (const json::value& level : array) {
            levels.push_back(parseBookLevel(level));
        }

        return levels;
    }

    binance::DepthUpdate parseDepthUpdate(std::string_view message)
    {
        const json::value parsed = json::parse(message);
        const json::object& object = parsed.as_object();

        binance::DepthUpdate update;
        update.firstUpdateId = jsonUnsigned(object.at("U"));
        update.lastUpdateId = jsonUnsigned(object.at("u"));
        update.exchangeTimestamp = jsonUnsigned(object.at("E"));
        update.bids = parseLevels(object.at("b"));
        update.asks = parseLevels(object.at("a"));

        return update;
    }

    binance::Trade parseTrade(std::string_view message)
    {
        const json::value parsed = json::parse(message);
        const json::object& object = parsed.as_object();

        binance::Trade trade;
        trade.sequence = jsonUnsigned(object.at("t"));
        trade.exchangeTimestamp = jsonUnsigned(object.at("T"));
        trade.price = parseDecimal(jsonString(object.at("p")));
        trade.quantity = parseDecimal(jsonString(object.at("q")));

        // "m" is true when the buyer was the market maker.
        // Therefore, the seller was the aggressor in that case.
        const json::value& buyerIsMaker = object.at("m");

        if (!buyerIsMaker.is_bool()) {
            throw std::runtime_error("Invalid trade maker flag");
        }

        trade.side = buyerIsMaker.as_bool() ? binance::Side::Sell : binance::Side::Buy;
        return trade;
    }

    binance::Snapshot parseSnapshot(std::string_view message)
    {
        const json::value parsed = json::parse(message);
        const json::object& object = parsed.as_object();

        binance::Snapshot snapshot;
        snapshot.lastUpdateId = jsonUnsigned(object.at("lastUpdateId"));
        snapshot.bids = parseLevels(object.at("bids"));
        snapshot.asks = parseLevels(object.at("asks"));

        return snapshot;
    }

    void setSni(beast::ssl_stream<beast::tcp_stream>& stream, const std::string& host)
    {
        if (SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()) != 1) {
            throw std::runtime_error("Failed to configure TLS SNI");
        }
    }

    template<typename Stream>
    void configureTls(Stream& stream, const std::string& host)
    {
        if (SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()) != 1) {
            throw std::runtime_error("Failed to configure TLS SNI");
        }

        stream.next_layer().set_verify_callback(ssl::host_name_verification(host));
    }
}

namespace binance
{
    class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
    {
    public:
        using MessageHandler = std::function<void(std::string)>;

        WebSocketSession(net::io_context& ioContext,
                         ssl::context& sslContext,
                         std::string streamName,
                         MessageHandler handler)
            : resolver(ioContext),
              stream(ioContext, sslContext),
              streamName(std::move(streamName)),
              handler(std::move(handler))
        {
        }

        void start()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            resolver.async_resolve(std::string(BinanceHost), std::string(HttpsPort),
                [self](const beast::error_code& errorCode, const Tcp::resolver::results_type& endpoints)
                {
                    if (errorCode) {
                        self->reportError("WebSocket resolve failed", errorCode);
                        return;
                    }

                    beast::get_lowest_layer(self->stream).async_connect(endpoints,
                        [self](const beast::error_code& connectError, const Tcp::endpoint&)
                        {
                            if (connectError) {
                                self->reportError("WebSocket connect failed", connectError);
                                return;
                            }

                            self->onConnected();
                        });
                });
        }

        void stop()
        {
            beast::error_code errorCode;
            beast::get_lowest_layer(stream).socket().cancel(errorCode);
            beast::get_lowest_layer(stream).socket().close(errorCode);
        }

    private:
        void onConnected()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            try {
                configureTls(stream.next_layer(), std::string(BinanceHost));
            } catch (const std::exception& exception) {
                std::cerr << exception.what() << '\n';
                stop();
                return;
            }

            stream.next_layer().async_handshake(ssl::stream_base::client,
                [self](const beast::error_code& errorCode)
                {
                    if (errorCode) {
                        self->reportError("WebSocket TLS handshake failed", errorCode);
                        return;
                    }

                    self->onTlsHandshake();
                });
        }

        void onTlsHandshake()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            stream.async_handshake(std::string(BinanceHost), "/ws",
                [self](const beast::error_code& errorCode)
                {
                    if (errorCode) {
                        self->reportError("WebSocket handshake failed", errorCode);
                        return;
                    }

                    self->subscribe();
                });
        }

        void subscribe()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            subscription = "{\"method\":\"SUBSCRIBE\",\"params\":[\"" +
                streamName + "\"],\"id\":1}";

            stream.async_write(net::buffer(subscription),
                [self](const beast::error_code& errorCode, std::size_t)
                {
                    if (errorCode) {
                        self->reportError("WebSocket subscription failed", errorCode);
                        return;
                    }

                    self->read();
                });
        }

        void read()
        {
            const std::shared_ptr<WebSocketSession> self = shared_from_this();

            stream.async_read(buffer,
                [self](const beast::error_code& errorCode, std::size_t)
                {
                    if (errorCode) {
                        if (errorCode != websocket::error::closed &&
                            errorCode != net::error::operation_aborted) {
                            self->reportError("WebSocket read failed", errorCode);
                        }

                        return;
                    }

                    std::string message = beast::buffers_to_string(self->buffer.data());
                    self->buffer.consume(self->buffer.size());

                    if (self->handler) {
                        self->handler(std::move(message));
                    }

                    self->read();
                });
        }

        void reportError(const char* operation, const beast::error_code& errorCode)
        {
            if (errorCode != net::error::operation_aborted) {
                std::cerr << operation << ": " << errorCode.message() << '\n';
            }
        }

        Tcp::resolver resolver;
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> stream;
        beast::flat_buffer buffer;
        std::string streamName;
        std::string subscription;
        MessageHandler handler;
    };

    class SnapshotRequest : public std::enable_shared_from_this<SnapshotRequest>
    {
    public:
        SnapshotRequest(net::io_context& ioContext,
                        ssl::context& sslContext,
                        std::string symbol,
                        BinanceConnector::SnapshotHandler handler)
            : resolver(ioContext),
              stream(ioContext, sslContext),
              symbol(std::move(symbol)),
              handler(std::move(handler))
        {
        }

        void start()
        {
            const std::shared_ptr<SnapshotRequest> self = shared_from_this();

            resolver.async_resolve(std::string(BinanceRestHost), std::string(HttpsPort),
                [self](const beast::error_code& errorCode, const Tcp::resolver::results_type& endpoints)
                {
                    if (errorCode) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::ResolveFailed, errorCode.message()
                        }));
                        return;
                    }

                    beast::get_lowest_layer(self->stream).async_connect(endpoints,
                        [self](const beast::error_code& connectError, const Tcp::endpoint&)
                        {
                            if (connectError) {
                                self->finish(std::unexpected(RestFailure {
                                    RestError::ConnectFailed, connectError.message()
                                }));
                                return;
                            }

                            self->onConnected();
                        });
                });
        }

    private:
        void onConnected()
        {
            try {
                configureTls(stream, std::string(BinanceRestHost));
            } catch (const std::exception& exception) {
                finish(std::unexpected(RestFailure {
                    RestError::TlsHandshakeFailed, exception.what()
                }));
                return;
            }

            const std::shared_ptr<SnapshotRequest> self = shared_from_this();

            stream.async_handshake(ssl::stream_base::client,
                [self](const beast::error_code& errorCode)
                {
                    if (errorCode) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::TlsHandshakeFailed, errorCode.message()
                        }));
                        return;
                    }

                    self->sendRequest();
                });
        }

        void sendRequest()
        {
            request.version(11);
            request.method(http::verb::get);
            request.target("/api/v3/depth?symbol=" + symbol + "&limit=1000");
            request.set(http::field::host, BinanceRestHost);
            request.set(http::field::user_agent, "BinanceConnector/1.0");
            request.set(http::field::accept, "application/json");

            const std::shared_ptr<SnapshotRequest> self = shared_from_this();

            http::async_write(stream, request,
                [self](const beast::error_code& errorCode, std::size_t)
                {
                    if (errorCode) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::WriteFailed, errorCode.message()
                        }));
                        return;
                    }

                    self->readResponse();
                });
        }

        void readResponse()
        {
            const std::shared_ptr<SnapshotRequest> self = shared_from_this();

            http::async_read(stream, buffer, response,
                [self](const beast::error_code& errorCode, std::size_t)
                {
                    if (errorCode) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::ReadFailed, errorCode.message()
                        }));
                        return;
                    }

                    if (self->response.result() != http::status::ok) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::InvalidResponse,
                            "HTTP status: " + std::to_string(
                                self->response.result_int()) + ", body: " + self->response.body()
                        }));
                        return;
                    }

                    try {
                        Snapshot snapshot = parseSnapshot(self->response.body());
                        self->finish(std::move(snapshot));
                    } catch (const std::exception& exception) {
                        self->finish(std::unexpected(RestFailure {
                            RestError::InvalidResponse, exception.what()
                        }));
                    }
                });
        }

        void finish(std::expected<Snapshot, RestFailure> result)
        {
            if (completed) {
                return;
            }

            completed = true;

            if (handler) {
                handler(std::move(result));
            }
        }

        Tcp::resolver resolver;
        SslStream stream;
        beast::flat_buffer buffer;
        http::request<http::empty_body> request;
        http::response<http::string_body> response;
        std::string symbol;
        BinanceConnector::SnapshotHandler handler;
        bool completed { false };
    };

    BinanceConnector::BinanceConnector(net::io_context& ioContext, std::string symbol)
        : ioContext(ioContext),
          sslContext(ssl::context::tls_client),
          symbol(toUpper(std::move(symbol)))
    {
        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(ssl::verify_peer);
    }

    BinanceConnector::~BinanceConnector()
    {
        stop();
    }

    void BinanceConnector::startDepth(DepthHandler handler)
    {
        stopDepth();
        depthHandler = std::move(handler);

        const std::string streamName = toLower(symbol) + "@depth@100ms";

        depthSession = std::make_shared<WebSocketSession>(
            ioContext,
            sslContext,
            streamName,
            [this](std::string message)
            {
                onDepthMessage(std::move(message));
            });

        depthSession->start();
    }

    void BinanceConnector::startTrades(TradeHandler handler)
    {
        stopTrades();
        tradeHandler = std::move(handler);

        const std::string streamName = toLower(symbol) + "@trade";

        tradeSession = std::make_shared<WebSocketSession>(
            ioContext,
            sslContext,
            streamName,
            [this](std::string message)
            {
                onTradeMessage(std::move(message));
            });

        tradeSession->start();
    }

    void BinanceConnector::asyncSnapshot(SnapshotHandler handler)
    {
        auto request = std::make_shared<SnapshotRequest>(
            ioContext, sslContext, symbol, std::move(handler));

        request->start();
    }

    void BinanceConnector::stopDepth()
    {
        if (depthSession) {
            depthSession->stop();
            depthSession.reset();
        }

        depthHandler = {};
    }

    void BinanceConnector::stopTrades()
    {
        if (tradeSession) {
            tradeSession->stop();
            tradeSession.reset();
        }

        tradeHandler = {};
    }

    void BinanceConnector::stop()
    {
        stopDepth();
        stopTrades();
    }

    void BinanceConnector::onDepthMessage(std::string message)
    {
        try {
            const DepthUpdate update = parseDepthUpdate(message);

            if (depthHandler) {
                depthHandler(update);
            }
        } catch (const std::exception& exception) {
            std::cerr << "Failed to parse depth update: " << exception.what() << '\n';
        }
    }

    void BinanceConnector::onTradeMessage(std::string message)
    {
        try {
            const Trade trade = parseTrade(message);

            if (tradeHandler) {
                tradeHandler(trade);
            }
        } catch (const std::exception& exception) {
            std::cerr << "Failed to parse trade: " << exception.what() << '\n';
        }
    }
}
