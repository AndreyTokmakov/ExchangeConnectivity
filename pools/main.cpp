/**============================================================================
Name        : Utilities.cpp
Created on  : 29.02.2024
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description : C++ Utilities
============================================================================**/

#include <iostream>
#include <string_view>
#include <vector>
#include <mutex>
#include <utility>

namespace book_updates_pool
{
    enum class Side : std::uint8_t
    {
        Buy,
        Sell
    };

    struct PriceLevel
    {
        Side side {};
        std::int64_t price {};
        std::int64_t quantity {};
    };

    struct BookUpdate
    {
        std::uint64_t exchangeTimestamp {};
        std::uint64_t firstSequence {};
        std::uint64_t lastSequence {};
        std::vector<PriceLevel> updates;
    };

    class BookUpdatePool
    {
    public:
        class Lease
        {
            friend class BookUpdatePool;

        public:
            Lease() = default;

            Lease(const Lease&) = delete;
            Lease& operator=(const Lease&) = delete;

            Lease(Lease&& other) noexcept:
                pool { std::exchange(other.pool, nullptr) },
                index { std::exchange(other.index, 0) }
            {
            }

            Lease& operator=(Lease&& other) noexcept
            {
                if (this != &other)
                {
                    reset();
                    pool = std::exchange(other.pool, nullptr);
                    index = std::exchange(other.index, 0);
                }
                return *this;
            }

            ~Lease() {
                reset();
            }

            [[nodiscard]]
            explicit operator bool() const noexcept {
                return pool != nullptr;
            }

            BookUpdate& operator*() const noexcept {
                return pool->objects[index];
            }

            BookUpdate* operator->() const noexcept {
                return &pool->objects[index];
            }

        private:
            Lease(BookUpdatePool* pool, const std::size_t index) noexcept:
                pool { pool }, index { index }
            {
            }

            void reset() noexcept
            {
                if (pool != nullptr)
                {
                    pool->release(index);
                    pool = nullptr;
                }
            }

            BookUpdatePool* pool {};
            std::size_t index { 0 };
        };

        explicit BookUpdatePool(const std::size_t objectCount,
                                const std::size_t initialUpdateCapacity = 0)
        {
            objects.reserve(objectCount);
            freeIndices.reserve(objectCount);

            for (std::size_t index = 0; index < objectCount; ++index)
            {
                objects.emplace_back();
                objects.back().updates.reserve(initialUpdateCapacity);
                freeIndices.push_back(index);
            }
        }

        [[nodiscard]]
        Lease acquire() noexcept
        {
            std::lock_guard lock(mutex);
            if (freeIndices.empty()) {
                return {};
            }

            const std::size_t index = freeIndices.back();
            freeIndices.pop_back();

            return Lease { this, index };
        }

        [[nodiscard]]
        std::size_t available() const noexcept
        {
            std::lock_guard lock(mutex);
            return freeIndices.size();
        }

    private:

        void release(const std::size_t index) noexcept
        {
            BookUpdate& update = objects[index];

            update.updates.clear();
            update.exchangeTimestamp = 0;
            update.firstSequence = 0;
            update.lastSequence = 0;

            std::lock_guard lock(mutex);
            freeIndices.push_back(index);
        }

        std::vector<BookUpdate> objects;
        std::vector<std::size_t> freeIndices;

        mutable std::mutex mutex;
    };

    class OrderBookBuilder
    {
    public:
        static void process(const BookUpdatePool::Lease &update)
        {
            if (!update) {
                return;
            }
            std::cout << "Processing update: " << update->firstSequence << " - "
                      << update->lastSequence << ", levels: "
                      << update->updates.size() << ", capacity: "
                      << update->updates.capacity()
                      << '\n';

            // Apply updates to the order book.
            // The Lease is returned to the pool when this method ends.
        }
    };

    void test()
    {
        BookUpdatePool pool { 2, 4 };
        OrderBookBuilder builder;

        for (std::uint64_t sequence = 1; sequence <= 5; ++sequence)
        {
            auto update = pool.acquire();
            if (!update)
            {
                std::cerr << "No free BookUpdate objects\n";
                return;
            }

            update->exchangeTimestamp = sequence * 1'000'000;
            update->firstSequence = sequence * 10;
            update->lastSequence = sequence * 10 + 5;

            update->updates.emplace_back(Side::Buy, 6712300000000, 50000000);
            update->updates.emplace_back(Side::Sell, 6712400000000, 30000000);

            std::cout << "Before transfer: capacity = " << update->updates.capacity()
                      << ", size = "<< update->updates.size()
                      << '\n';

            builder.process(std::move(update));
            std::cout << "Available objects: " << pool.available()<< "\n\n";
        }
    }
}



int main([[maybe_unused]] int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    book_updates_pool::test();

    return EXIT_SUCCESS;
}
