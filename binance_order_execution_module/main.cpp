/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : Binance
============================================================================**/

#include <string_view>
#include <vector>
#include <iostream>

#include "networking/web_socket_client.hpp"
#include "networking/rest_client.hpp"
#include "utilities/utilities.hpp"

#include "common/price.hpp"
#include "common/quantity.hpp"
#include "common/timestamp.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http.hpp>

namespace
{
    struct ApiCredentials
    {
        std::string apiKey;
        std::string secretKey;
    };

    [[nodiscard]]
    std::int64_t getCurrentTimestamp()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    [[nodiscard]]
    std::string escapeJsonString(const std::string_view value)
    {
        std::string result;
        result.reserve(value.size());

        for (const char ch : value) {
            switch (ch) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b";  break;
                case '\f': result += "\\f";  break;
                case '\n': result += "\\n";  break;
                case '\r': result += "\\r";  break;
                case '\t': result += "\\t";  break;
                default:   result += ch;    break;
            }
        }

        return result;
    }

    std::optional<std::string> createSignature(const std::string_view payload,
                                               const std::string_view secretKey)
    {
        if (secretKey.size() > static_cast<std::size_t>( std::numeric_limits<int>::max())) {
            return std::nullopt;
        }

        std::array<unsigned char, EVP_MAX_MD_SIZE> digest {};
        unsigned int digestLength = 0;
        const unsigned char* result = HMAC(EVP_sha256(),
                                           secretKey.data(),
                                           static_cast<int>(secretKey.size()),
                                           reinterpret_cast<const unsigned char*>(payload.data()),
                                           payload.size(),
                                           digest.data(),
                                           &digestLength);
        if (nullptr == result) {
            return std::nullopt;
        }
        return utilities::toHex(std::span { digest.data(), digestLength });
    }

    std::optional<std::string> getEnvironmentVariable(const std::string_view variable) {
        if (const char* env_p = std::getenv(variable.data()))
            return env_p;
        return std::nullopt;
    }

    [[nodiscard]]
    std::optional<ApiCredentials> loadApiCredentials()
    {
        return ApiCredentials {
            .apiKey = getEnvironmentVariable("BINANCE_API_KEY").value_or(""),
            .secretKey = getEnvironmentVariable("BINANCE_SECRET_KEY").value_or("")
        };
    }
}

namespace binance
{
    class BinanceUserDataStream
    {
    public:
        using MessageHandler = networking::WebSocketClient::MessageHandler;

        static constexpr std::string_view Host = "ws-api.testnet.binance.vision";
        static constexpr std::string_view Port = "443";
        static constexpr std::string_view Target = "/ws-api/v3";

        static constexpr std::int64_t RecvWindow = 5000;

        BinanceUserDataStream(boost::asio::io_context& ioContext,
                              boost::asio::ssl::context& sslContext,
                              std::string apiKey,
                              std::string secretKey);

        ~BinanceUserDataStream();

        BinanceUserDataStream(const BinanceUserDataStream&) = delete;
        BinanceUserDataStream& operator=(const BinanceUserDataStream&) = delete;

        [[nodiscard]]
        bool start(MessageHandler handler);

        void stop();

    private:
        [[nodiscard]]
        std::optional<std::string> createSubscribeRequest() const;

        std::string apiKey;
        std::string secretKey;

        networking::WebSocketClient webSocketClient;
    };
}

namespace binance
{
    BinanceUserDataStream::BinanceUserDataStream(boost::asio::io_context& ioContext,
                                                 boost::asio::ssl::context& sslContext,
                                                 std::string apiKey,
                                                 std::string secretKey):
        apiKey(std::move(apiKey)),
        secretKey(std::move(secretKey)),
        webSocketClient(ioContext, sslContext, std::string { Host }, std::string { Port }, std::string { Target })
    {
    }

    BinanceUserDataStream::~BinanceUserDataStream()
    {
        stop();
    }

    bool BinanceUserDataStream::start(MessageHandler handler)
    {
        if (apiKey.empty() || secretKey.empty() || !handler) {
            return false;
        }

        std::optional<std::string> request = createSubscribeRequest();
        if (!request.has_value()) {
            return false;
        }

        webSocketClient.start(std::move(handler));
        webSocketClient.send(std::move(*request));

        return true;
    }

    void BinanceUserDataStream::stop()
    {
        webSocketClient.stop();
    }

    std::optional<std::string> BinanceUserDataStream::createSubscribeRequest() const
    {
        const std::int64_t timestamp = getCurrentTimestamp();

        // Binance requires parameters to be sorted alphabetically when calculating the signature.
        const std::string payload =
            "apiKey=" + apiKey +
            "&recvWindow=" + std::to_string(RecvWindow) +
            "&timestamp=" + std::to_string(timestamp);

        const std::optional<std::string> signature = createSignature(payload, secretKey);
        if (!signature.has_value()) {
            return std::nullopt;
        }

        std::string request =
            R"({"id":"1","method":"userDataStream.subscribe.signature","params":{)"
            R"("apiKey":")" + escapeJsonString(apiKey) +
            R"(","recvWindow":)" + std::to_string(RecvWindow) +
            R"(,"timestamp":)" + std::to_string(timestamp) +
            R"(,"signature":")" + *signature +
            R"("}})";

        return request;
    }
}

namespace
{
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace asio  = boost::asio;
    namespace ssl   = asio::ssl;

    void run()
    {
        const std::optional<ApiCredentials> credentials = loadApiCredentials();
        if (!credentials) {
            std::cerr << "Binance API credentials are not configured\n";
            return;
        }

        asio::io_context ioContext;
        ssl::context sslContext { ssl::context::tls_client};

        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(ssl::verify_peer);

        binance::BinanceUserDataStream userDataStream {
            ioContext,
            sslContext,
            credentials->apiKey,
            credentials->secretKey
        };

        if (!userDataStream.start([](const std::string& message) {
            std::cout << "[Binance User Data Stream] " << message << '\n';
        })) {
            std::cerr << "Failed to start Binance User Data Stream\n";
            return ;
        }

        std::cout << "Starting Binance User Data Stream...\n";
        ioContext.run();

        std::cout << "Binance User Data Stream stopped\n";
    }
}

int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    run();

    return EXIT_SUCCESS;
}

#if 0
---------------------- when creating new order ------------------------

{
  "subscriptionId": 0,
  "event": {
    "e": "executionReport",
    "E": 1793899739726,
    "s": "BTCUSDT",
    "c": "10001",
    "S": "BUY",
    "o": "LIMIT",
    "f": "GTC",
    "q": "0.00100000",
    "p": "75000.00000000",
    "P": "0.00000000",
    "F": "0.00000000",
    "g": -1,
    "C": "",
    "x": "NEW",
    "X": "NEW",
    "r": "NONE",
    "i": 10887393,
    "l": "0.00000000",
    "z": "0.00000000",
    "L": "0.00000000",
    "n": "0",
    "N": null,
    "T": 1793899739726,
    "t": -1,
    "I": 23931834,
    "w": true,
    "m": false,
    "M": false,
    "O": 1793899739726,
    "Z": "0.00000000",
    "Y": "0.00000000",
    "Q": "0.00000000",
    "W": 1793899739726,
    "V": "EXPIRE_MAKER"
  }
}

{
  "subscriptionId": 0,
  "event": {
    "e": "outboundAccountPosition",
    "E": 1793899739726,
    "u": 1793899739726,
    "B": [
      {
        "a": "BTC",
        "f": "1.00000000",
        "l": "0.00000000"
      },
      {
        "a": "USDT",
        "f": "9925.00000000",
        "l": "75.00000000"
      }
    ]
  }
}

---------------------- when canceling oreder ------------------------

{
  "subscriptionId": 0,
  "event": {
    "e": "executionReport",
    "E": 1793899842424,
    "s": "BTCUSDT",
    "c": "DIh1x4bk1SehYll5hmo3Rp",
    "S": "BUY",
    "o": "LIMIT",
    "f": "GTC",
    "q": "0.00100000",
    "p": "75000.00000000",
    "P": "0.00000000",
    "F": "0.00000000",
    "g": -1,
    "C": "10001",
    "x": "CANCELED",
    "X": "CANCELED",
    "r": "NONE",
    "i": 10887393,
    "l": "0.00000000",
    "z": "0.00000000",
    "L": "0.00000000",
    "n": "0",
    "N": null,
    "T": 1793899842424,
    "t": -1,
    "I": 23932929,
    "w": false,
    "m": false,
    "M": false,
    "O": 1793899739726,
    "Z": "0.00000000",
    "Y": "0.00000000",
    "Q": "0.00000000",
    "W": 1793899739726,
    "V": "EXPIRE_MAKER"
  }
}

{
  "subscriptionId": 0,
  "event": {
    "e": "outboundAccountPosition",
    "E": 1793899842424,
    "u": 1793899842424,
    "B": [
      {
        "a": "BTC",
        "f": "1.00000000",
        "l": "0.00000000"
      },
      {
        "a": "USDT",
        "f": "10000.00000000",
        "l": "0.00000000"
      }
    ]
  }
}
#endif

