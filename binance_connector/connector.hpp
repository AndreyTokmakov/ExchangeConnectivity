
#ifndef EXCHANGECONNECTIVITY_BINANCE_CONNECTOR_HPP
#define EXCHANGECONNECTIVITY_BINANCE_CONNECTOR_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace binance
{
    using Price = std::int64_t;
    using Quantity = std::int64_t;
    using SequenceNumber = std::uint64_t;
    using Timestamp = std::uint64_t;

    inline constexpr Price DecimalScale = 100'000'000;

    enum class Side : std::uint8_t
    {
        Buy,
        Sell
    };

    enum class RestError : std::uint8_t
    {
        ResolveFailed,
        ConnectFailed,
        TlsHandshakeFailed,
        WriteFailed,
        ReadFailed,
        InvalidResponse
    };

    struct RestFailure
    {
        RestError error { RestError::InvalidResponse };
        std::string message;
    };

    struct BookLevel
    {
        Price price { 0 };
        Quantity quantity { 0 };
    };

    struct Snapshot
    {
        SequenceNumber lastUpdateId { 0 };
        std::vector<BookLevel> bids;
        std::vector<BookLevel> asks;
    };

    struct DepthUpdate
    {
        SequenceNumber firstUpdateId { 0 };
        SequenceNumber lastUpdateId { 0 };
        Timestamp exchangeTimestamp { 0 };
        std::vector<BookLevel> bids;
        std::vector<BookLevel> asks;
    };

    struct Trade
    {
        SequenceNumber sequence { 0 };
        Timestamp exchangeTimestamp { 0 };
        Price price { 0 };
        Quantity quantity { 0 };
        Side side { Side::Buy };
    };

    class WebSocketSession;
    class SnapshotRequest;

    class BinanceConnector
    {
    public:
        using DepthHandler = std::function<void(const DepthUpdate&)>;
        using TradeHandler = std::function<void(const Trade&)>;
        using SnapshotHandler = std::function<void(std::expected<Snapshot, RestFailure>)>;

        BinanceConnector(boost::asio::io_context& ioContext, std::string symbol);
        ~BinanceConnector();

        BinanceConnector(const BinanceConnector&) = delete;
        BinanceConnector& operator=(const BinanceConnector&) = delete;

        void startDepth(DepthHandler handler);
        void startTrades(TradeHandler handler);
        void asyncSnapshot(SnapshotHandler handler);

        void stopDepth();
        void stopTrades();
        void stop();

    private:
        void onDepthMessage(const std::string& message) const;
        void onTradeMessage(const std::string& message) const;

        boost::asio::io_context& ioContext;
        boost::asio::ssl::context sslContext;
        std::string symbol;

        std::shared_ptr<WebSocketSession> depthSession;
        std::shared_ptr<WebSocketSession> tradeSession;

        DepthHandler depthHandler;
        TradeHandler tradeHandler;
    };
}

#endif
