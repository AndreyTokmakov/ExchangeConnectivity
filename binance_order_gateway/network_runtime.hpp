/**============================================================================
Name        : network_runtime.hpp
Created on  : 17.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : network_runtime.hpp
============================================================================**/

#ifndef EXCHANGECONNECTIVITY_NETWORK_RUNTIME_HPP
#define EXCHANGECONNECTIVITY_NETWORK_RUNTIME_HPP

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <thread>
#include <vector>

namespace trading::networking
{
    class NetworkRuntime
    {
    public:
        explicit NetworkRuntime(std::size_t threadCount = 1);

        ~NetworkRuntime();

        NetworkRuntime(const NetworkRuntime&) = delete;
        NetworkRuntime& operator=(const NetworkRuntime&) = delete;

        NetworkRuntime(NetworkRuntime&&) = delete;
        NetworkRuntime& operator=(NetworkRuntime&&) = delete;

        [[nodiscard]]
        boost::asio::io_context& getIoContext() noexcept;

        [[nodiscard]]
        boost::asio::ssl::context& getSslContext() noexcept;

        void stop();

    private:
        using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

        boost::asio::io_context ioContext;
        boost::asio::ssl::context sslContext;
        WorkGuard workGuard;

        std::vector<std::thread> networkThreads;
        bool stopped { false };
    };
}


#endif //EXCHANGECONNECTIVITY_NETWORK_RUNTIME_HPP
