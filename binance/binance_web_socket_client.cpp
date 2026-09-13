#include "binance_web_socket_client.hpp"

#include <boost/asio/connect.hpp>

#include <openssl/ssl.h>

#include <utility>

namespace exchange::binance
{
    BinanceWebSocketClient::BinanceWebSocketClient(asio::io_context& ioContext,
                                                   asio::ssl::context& sslContext,
                                                   MessageHandler messageHandler,
                                                   ErrorHandler errorHandler):
        resolver(asio::make_strand(ioContext)),
        webSocket(asio::make_strand(ioContext), sslContext),
        messageHandler(std::move(messageHandler)),
        errorHandler(std::move(errorHandler))
    {
        webSocket.control_callback([this](beast::websocket::frame_type frameType, beast::string_view){
            if (frameType == beast::websocket::frame_type::ping){
                webSocket.pong({});
            }
        });
    }

    void BinanceWebSocketClient::connect(const std::string& host,
                                         const std::string& port,
                                         const std::string& target)
    {
        this->host = host;
        this->target = target;

        resolver.async_resolve(host, port, [this](const error_code &errorCode,
                  const tcp::resolver::results_type& results) {
            onResolve(errorCode, results);
        });
    }

    void BinanceWebSocketClient::send(const std::string_view message)
    {
        writeQueue.emplace_back(message);
        if (state == State::Connected) {
            write();
        }
    }

    void BinanceWebSocketClient::onResolve(const error_code &errorCode,
                                           const tcp::resolver::results_type& results)
    {
        if (errorCode)
        {
            if (errorHandler) {
                errorHandler(errorCode);
            }

            return;
        }

        asio::async_connect(webSocket.next_layer().next_layer(), results,
            [this](const error_code& err, const tcp::endpoint&) {
                onConnect(err);
        });
    }

    void BinanceWebSocketClient::onConnect(const error_code &errorCode)
    {
        if (errorCode)
        {
            if (errorHandler){
                errorHandler(errorCode);
            }

            return;
        }

        if (!SSL_set_tlsext_host_name(webSocket.next_layer().native_handle(), host.c_str()))
        {
            const error_code sslError{static_cast<int>(::ERR_get_error()),boost::asio::error::get_ssl_category()};
            if (errorHandler){
                errorHandler(sslError);
            }

            return;
        }

        webSocket.next_layer().async_handshake(asio::ssl::stream_base::client, [this](const error_code &errorCode) {
            onSslHandshake(errorCode);
        });
    }

    void BinanceWebSocketClient::onSslHandshake(const error_code &errorCode)
    {
        if (errorCode){
            if (errorHandler){
                errorHandler(errorCode);
            }

            return;
        }

        webSocket.async_handshake(host,target,[this](const error_code &errorCode){
            onWebSocketHandshake(errorCode);
        });
    }

    void BinanceWebSocketClient::onWebSocketHandshake(error_code errorCode)
    {
        if (errorCode) {
            if (errorHandler) {
                errorHandler(errorCode);
            }

            return;
        }

        state = State::Connected;
        if (!writeQueue.empty()) {
            write();
        }

        read();
    }

    void BinanceWebSocketClient::write()
    {
        if (state != State::Connected || writeQueue.empty()) {
            return;
        }

        state = State::Writing;
        webSocket.text(true);

        webSocket.async_write(asio::buffer(writeQueue.front()),
            [this](const error_code &errorCode, const std::size_t bytesTransferred)
        {
            onWrite(errorCode, bytesTransferred);
        });
    }

    void BinanceWebSocketClient::onWrite(const error_code &errorCode,
                                         std::size_t)
    {
        state = State::Disconnected;
        if (errorCode)
        {
            if (errorHandler){
                errorHandler(errorCode);
            }

            return;
        }

        writeQueue.pop_front();

        if (!writeQueue.empty())
        {
            write();
        }
    }

    void BinanceWebSocketClient::read()
    {
        webSocket.async_read(readBuffer,[this](const error_code& errorCode, const std::size_t bytesTransferred){
            onRead(errorCode, bytesTransferred);
        });
    }

    void BinanceWebSocketClient::onRead(error_code errorCode,
                                        std::size_t)
    {
        if (errorCode)
        {
            onClose(errorCode);
            return;
        }

        const auto data = readBuffer.data();
        const std::string_view message{static_cast<const char*>(data.data()),readBuffer.size() };
        if (messageHandler){
            messageHandler(message);
        }

        readBuffer.consume(readBuffer.size());
        read();
    }

    void BinanceWebSocketClient::close()
    {
        if (State::Connected != state) {
            return;
        }

        error_code errorCode;
        webSocket.close(beast::websocket::close_code::normal,errorCode);

        if (errorCode){
            onClose(errorCode);
        }
    }

    void BinanceWebSocketClient::onClose(error_code errorCode)
    {
        state = State::Disconnected;
        if (errorCode != beast::websocket::error::closed && errorHandler){
            errorHandler(errorCode);
        }
    }
}