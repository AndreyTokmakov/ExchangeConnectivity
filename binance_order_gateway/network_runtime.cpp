/**============================================================================
Name        : network_runtime.cpp
Created on  : 17.11.2026
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : network_runtime.cpp
============================================================================**/

#include "network_runtime.hpp"


#include <stdexcept>

namespace trading::networking
{
    NetworkRuntime::NetworkRuntime(const std::size_t threadCount):
        sslContext { boost::asio::ssl::context::tls_client },
        workGuard { boost::asio::make_work_guard(ioContext) }
    {
        if (threadCount == 0) {
            throw std::invalid_argument("Network thread count must be greater than zero");
        }

        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(boost::asio::ssl::verify_peer);

        networkThreads.reserve(threadCount);

        try
        {
            for (std::size_t index = 0; index < threadCount; ++index) {
                networkThreads.emplace_back([this](){
                    ioContext.run();
                });
            }
        }
        catch (...)
        {
            workGuard.reset();
            ioContext.stop();

            for (std::thread& thread : networkThreads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
            throw;
        }
    }

    NetworkRuntime::~NetworkRuntime()
    {
        stop();
    }

    boost::asio::io_context& NetworkRuntime::getIoContext() noexcept
    {
        return ioContext;
    }

    boost::asio::ssl::context& NetworkRuntime::getSslContext() noexcept
    {
        return sslContext;
    }

    void NetworkRuntime::stop()
    {
        if (stopped) {
            return;
        }

        for (const std::thread& thread : networkThreads)
        {
            if (thread.get_id() == std::this_thread::get_id()) {
                throw std::logic_error("NetworkRuntime cannot be stopped from its own thread");
            }
        }

        stopped = true;

        workGuard.reset();
        ioContext.stop();

        for (std::thread& thread : networkThreads)
        {
            if (thread.joinable()) {
                thread.join();
            }
        }

        networkThreads.clear();
    }
}
