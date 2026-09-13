#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <boost/beast/ssl/ssl_stream.hpp>


namespace exchange::binance
{
    namespace asio = boost::asio;
    namespace beast = boost::beast;

    using tcp = asio::ip::tcp;
    using error_code = beast::error_code;

    class BinanceWebSocketClient
    {
    public:
        using MessageHandler = std::function<void(std::string_view)>;
        using ErrorHandler = std::function<void(error_code)>;

        BinanceWebSocketClient(asio::io_context& ioContext,
                               asio::ssl::context& sslContext,
                               MessageHandler messageHandler,
                               ErrorHandler errorHandler = {});

        void connect(const std::string& host,
                     const std::string& port,
                     const std::string& target);

        void send(std::string_view message);

        void close();

    private:
        using WebSocketStream = beast::websocket::stream< beast::ssl_stream<tcp::socket>>;

        void onResolve(const error_code &errorCode, const tcp::resolver::results_type& results);

        void onConnect(const error_code &errorCode);

        void onSslHandshake(const error_code &errorCode);

        void onWebSocketHandshake(error_code errorCode);

        void read();

        void onRead(error_code errorCode, std::size_t bytesTransferred);

        void write();

        void onWrite(const error_code &errorCode, std::size_t bytesTransferred);

        void onClose(error_code errorCode);

    private:
        enum class State: uint8_t
        {
            Disconnected,
            Resolving,
            Connecting,
            TlsHandshake,
            WebSocketHandshake,
            Connected,
            Writing
        };

    private:
        tcp::resolver resolver;
        WebSocketStream webSocket;
        beast::flat_buffer readBuffer;

        std::string host;
        std::string target;

        std::deque<std::string> writeQueue;

        MessageHandler messageHandler;
        ErrorHandler errorHandler;

        State state { State::Disconnected };
    };
}