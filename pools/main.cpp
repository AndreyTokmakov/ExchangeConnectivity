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
#include <condition_variable>
#include <deque>

namespace
{
    enum class Side : std::uint8_t {
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
}

namespace book_updates_pool
{
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

        explicit BookUpdatePool(const std::size_t initialCapacity = 0,
                                const std::size_t initialUpdateCapacity = 0)
            : initialUpdateCapacity { initialUpdateCapacity }
        {
            objects.reserve(initialCapacity);
            freeIndices.reserve(initialCapacity);

            for (std::size_t index = 0; index < initialCapacity; ++index)
            {
                objects.emplace_back().updates.reserve(initialUpdateCapacity);
                freeIndices.push_back(index);
            }
        }

        [[nodiscard]]
        Lease acquire() noexcept
        {
            std::lock_guard lock(mutex);
            if (freeIndices.empty())
            {
                const std::size_t index = objects.size();
                objects.emplace_back().updates.reserve(initialUpdateCapacity);
                return Lease { this, index };
            }
            const std::size_t index = freeIndices.back();
            freeIndices.pop_back();

            return Lease { this, index };
        }

        [[nodiscard]]
        std::size_t size() const
        {
            std::lock_guard lock(mutex);
            return objects.size();
        }

        [[nodiscard]]
        std::size_t available() const
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
        std::size_t initialUpdateCapacity {};
    };

    class OrderBookBuilder
    {
    public:
        static void process(BookUpdatePool::Lease && update)
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
            BookUpdatePool::Lease update = pool.acquire();
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


namespace book_updates_scsp_pool
{
 #ifdef __cpp_lib_hardware_interference_size
    inline constexpr std::size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
    inline constexpr std::size_t kCacheLineSize = 64;
#endif

    constexpr uint32_t fast_modulo(const uint32_t n, const uint32_t d) noexcept {
        return n & (d - 1);
    }

    constexpr bool is_pow_of_2(const int value) noexcept {
        return (value && !(value & (value - 1)));
    }

    template<typename Ty, uint32_t Capacity>
    class RingBuffer
    {
        using size_type  = int64_t;
        using value_type = Ty;
        using collection_type = std::vector<value_type>;

        static_assert(!std::is_same_v<value_type, void>, "ERROR: Value type can not be void");
        static_assert(is_pow_of_2(Capacity), "ERROR: Capacity must be a power of 2");
        static_assert(Capacity >= 2, "Buffer size must be at least 2");

    public:
        RingBuffer() = default;

        RingBuffer(const RingBuffer&) = delete;
        RingBuffer& operator=(const RingBuffer&) = delete;

        template <typename... Args>
        bool emplace(Args&&... args)
        {
            const size_type seq = writeCursor.load(std::memory_order_relaxed);
            /** Full when write is exactly N ahead of read **/
            if (seq - cachedReadCursor >= static_cast<size_type>(Capacity))
            {
                cachedReadCursor = readCursor.load(std::memory_order_acquire);
                if (seq - cachedReadCursor >= static_cast<size_type>(Capacity)) {
                    return false; // full
                }
            }

            // Construct the element directly in the slot
            const std::size_t idx = seq & kMask;
            buffer[idx].~value_type();
            new (&buffer[idx]) value_type(std::forward<Args>(args)...);

            // Publish: make the write visible to the consumer
            writeCursor.store(seq + 1, std::memory_order_release);
            return true;
        }

        bool push(const value_type& item)
        {
            const size_type seq = writeCursor.load(std::memory_order_relaxed);
            /** Full when write is exactly N ahead of read **/
            if (seq - cachedReadCursor >= static_cast<size_type>(Capacity))
            {
                cachedReadCursor = readCursor.load(std::memory_order_acquire);
                if (seq - cachedReadCursor >= static_cast<size_type>(Capacity)) {
                    return false; // full
                }
            }

            // Construct the element directly in the slot
            const std::size_t idx = seq & kMask;
            buffer[idx] = item;

            // Publish: make the write visible to the consumer
            writeCursor.store(seq + 1, std::memory_order_release);
            return true;
        }

        [[nodiscard]]
        bool pop(value_type& item)
        {
            const size_type seq = readCursor.load(std::memory_order_relaxed);

            // Empty when read has caught up to write
            if (seq >= cachedWriteCursor)
            {
                // Refresh our cached copy of the write cursor
                cachedWriteCursor = writeCursor.load(std::memory_order_acquire);
                if (seq >= cachedWriteCursor) {
                    return false;
                }
            }

            // Read from the slot
            const std::size_t idx = seq & kMask;
            item = std::move(buffer[idx]);

            // Advance read cursor
            readCursor.store(seq + 1, std::memory_order_release);
            return true;
        }

    private:
        static constexpr std::size_t kMask { Capacity - 1 };

        alignas(kCacheLineSize) std::array<value_type, Capacity> buffer{};

        // Producer's cache line
        alignas(kCacheLineSize) std::atomic<size_type> writeCursor { 0 };
        size_type cachedReadCursor { 0 };  // producer's local cache

        // Consumer's cache line
        alignas(kCacheLineSize) std::atomic<size_type> readCursor { 0 };
        size_type cachedWriteCursor { 0 }; // consumer's local cache
    };

    template<std::uint32_t Capacity>
    class BookUpdatePool
    {
        using Index = std::uint32_t;
        using FreeIndicesQueue = RingBuffer<Index, Capacity>;

        static_assert(Capacity >= 2);
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

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

            ~Lease(){
                reset();
            }

            [[nodiscard]]
            explicit operator bool() const noexcept{
                return pool != nullptr;
            }

            BookUpdate& operator*() const noexcept {
                return pool->objects[index];
            }

            BookUpdate* operator->() const noexcept {
                return &pool->objects[index];
            }

        private:
            Lease(BookUpdatePool* pool, const Index index) noexcept:
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
            Index index {};
        };

        explicit BookUpdatePool(const std::size_t initialUpdateCapacity = 0):
            objects(Capacity)
        {
            for (Index index = 0; index < Capacity; ++index)
            {
                objects[index].updates.reserve(initialUpdateCapacity);
                if (const bool inserted = freeIndices.emplace(index); !inserted)
                {
                    std::terminate();
                }
            }
        }

        BookUpdatePool(const BookUpdatePool&) = delete;
        BookUpdatePool& operator=(const BookUpdatePool&) = delete;

        [[nodiscard]]
        Lease acquire() noexcept
        {
            Index index {};
            if (!freeIndices.pop(index)){
                return {};
            }

            return Lease { this, index };
        }

        [[nodiscard]]
        static std::size_t size() noexcept
        {
            return Capacity;
        }

        [[nodiscard]]
        std::size_t available() const noexcept
        {
            return Capacity - acquiredCount.load(std::memory_order_relaxed);
        }

    private:
        void release(const Index index) noexcept
        {
            BookUpdate& update = objects[index];

            update.updates.clear();
            update.exchangeTimestamp = 0;
            update.firstSequence = 0;
            update.lastSequence = 0;

            if (const bool inserted = freeIndices.emplace(index); !inserted){
                std::terminate();
            }
        }

        std::vector<BookUpdate> objects {};
        FreeIndicesQueue freeIndices;

        std::atomic<std::size_t> acquiredCount { 0 };
    };
}

namespace perf_tests
{
    using Clock = std::chrono::steady_clock;

    constexpr std::size_t MessageCount = 5'000'000;
    constexpr std::size_t UpdateLevelCount = 50;
    constexpr std::size_t InitialPoolCapacity = 1024;

    template<typename T>
    class Queue
    {
    public:
        void push(T value)
        {
            {
                std::lock_guard lock(mutex);
                items.emplace_back(std::move(value));
            }

            condition.notify_one();
        }

        bool pop(T& value)
        {
            std::unique_lock lock(mutex);
            condition.wait(lock, [this]{
                return !items.empty() || closed;
            });

            if (items.empty()){
                return false;
            }

            value = std::move(items.front());
            items.pop_front();

            return true;
        }

        void close()
        {
            {
                std::lock_guard lock(mutex);
                closed = true;
            }
            condition.notify_all();
        }

        [[nodiscard]]
        std::size_t size() const
        {
            std::lock_guard lock(mutex);
            return items.size();
        }

    private:
        std::deque<T> items;
        mutable std::mutex mutex;
        std::condition_variable condition;
        bool closed {};
    };

    using book_updates_pool::BookUpdatePool;

    void produce(BookUpdate& update, const std::uint64_t sequence)
    {
        update.exchangeTimestamp = sequence;
        update.firstSequence = sequence;
        update.lastSequence = sequence;

        update.updates.clear();

        for (std::size_t index = 0; index < UpdateLevelCount; ++index)
        {
            update.updates.emplace_back(Side::Buy,
                static_cast<std::int64_t>(100'000 + index),
                static_cast<std::int64_t>(sequence + index)
            );
        }
    }

    [[nodiscard]]
    std::uint64_t consume(const BookUpdate& update)
    {
        std::uint64_t checksum = update.firstSequence;
        for (const auto& level : update.updates)
        {
            checksum += static_cast<std::uint64_t>(level.price);
            checksum += static_cast<std::uint64_t>(level.quantity);
        }
        return checksum;
    }


    struct TestResult
    {
        std::string_view name {};
        std::uint64_t elapsedNanoseconds {};
        std::uint64_t messagesProcessed {};
        std::uint64_t checksum {};
        std::size_t maxQueueSize {};
        std::size_t poolSize {};
    };

    void printResult(const TestResult& result)
    {
        const double elapsedSeconds = static_cast<double>(result.elapsedNanoseconds) / 1'000'000'000.0;
        const double messagesPerSecond = static_cast<double>(result.messagesProcessed) / elapsedSeconds;
        const double nanosecondsPerMessage = static_cast<double>(result.elapsedNanoseconds) /
            static_cast<double>(result.messagesProcessed);

        std::cout << "\nTest: " << result.name << '\n';
        std::cout << "Messages: " << result.messagesProcessed << '\n';
        std::cout << "Elapsed: " << elapsedSeconds << " seconds\n";
        std::cout << "Throughput: " << messagesPerSecond << " messages/sec\n";
        std::cout << "Latency: " << nanosecondsPerMessage << " ns/message\n";
        std::cout << "Max queue size: " << result.maxQueueSize << '\n';
        std::cout << "Pool size: " << result.poolSize << '\n';
        std::cout << "Checksum: " << result.checksum << '\n';
    }

    TestResult runBaseline()
    {
        Queue<BookUpdate> queue;

        TestResult result;
        result.name = "Baseline";

        std::uint64_t checksum {};

        const auto start = Clock::now();
        std::thread producer([&queue, &result]
        {
            for (std::uint64_t sequence = 1; sequence <= MessageCount; ++sequence)
            {
                BookUpdate update;
                update.updates.reserve(UpdateLevelCount);
                produce(update, sequence);
                queue.push(std::move(update));
                if (sequence % 100'000 == 0){
                    result.maxQueueSize = std::max(result.maxQueueSize,queue.size());
                }
            }

            queue.close();
        });

        BookUpdate update;

        while (queue.pop(update))
        {
            checksum += consume(update);
            ++result.messagesProcessed;
        }

        producer.join();

        result.elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        result.checksum = checksum;

        return result;
    }

    TestResult runPool()
    {
        BookUpdatePool pool { InitialPoolCapacity, UpdateLevelCount };
        Queue<BookUpdatePool::Lease> queue;

        TestResult result;
        result.name = "Pool";

        std::uint64_t checksum {};

        const auto start = Clock::now();
        std::thread producer([&queue, &pool, &result]
        {
            for (std::uint64_t sequence = 1; sequence <= MessageCount; ++sequence)
            {
                BookUpdatePool::Lease update = pool.acquire();
                produce(*update, sequence);
                queue.push(std::move(update));
                if (sequence % 100'000 == 0){
                    result.maxQueueSize = std::max(result.maxQueueSize,queue.size());
                }
            }

            queue.close();
        });

        BookUpdatePool::Lease update;

        while (queue.pop(update))
        {
            checksum += consume(*update);
            ++result.messagesProcessed;

            // Return the object to the pool.
            update = {};
        }

        producer.join();

        result.elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        result.checksum = checksum;
        result.poolSize = pool.size();

        return result;
    }

    TestResult runScSpPool()
    {
        book_updates_scsp_pool::BookUpdatePool<64 * 1024> pool;
        Queue<typename decltype(pool)::Lease> queue;

        TestResult result;
        result.name = "ScSpPool";

        std::uint64_t checksum {};

        const auto start = Clock::now();
        std::thread producer([&queue, &pool, &result]
        {
            for (std::uint64_t sequence = 1; sequence <= MessageCount; ++sequence)
            {
                decltype(pool)::Lease update = pool.acquire();
                produce(*update, sequence);
                queue.push(std::move(update));
                if (sequence % 100'000 == 0){
                    result.maxQueueSize = std::max(result.maxQueueSize,queue.size());
                }
            }

            queue.close();
        });

        decltype(pool)::Lease update;
        while (queue.pop(update))
        {
            checksum += consume(*update);
            ++result.messagesProcessed;

            // Return the object to the pool.
            update = {};
        }

        producer.join();

        result.elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        result.checksum = checksum;
        result.poolSize = pool.size();

        return result;
    }


    void run()
    {
        std::cout << "BookUpdate Pool Benchmark\n";
        std::cout << "Messages: " << MessageCount << '\n';
        std::cout << "Update levels: " << UpdateLevelCount << '\n';

        {
            const TestResult baseline = runBaseline();
            printResult(baseline);
        }
        {
            const TestResult pool = runPool();
            printResult(pool);
        }
        {
            const TestResult pool = runScSpPool();
            printResult(pool);
        }

        std::cout << "\nDone\n";
    }

#if 0
    BookUpdate Pool Benchmark
    Messages: 5000000
    Update levels: 50

    Test: Baseline
    Messages      : 5000000
    Elapsed       : 3.02299 seconds
    Throughput    : 1.65399e+06 messages/sec
    Latency       : 604.598 ns/message
    Max queue size: 24
    Pool size     : 0
    Checksum      : 662512377500000

    Test: Pool
    Messages: 5000000
    Elapsed: 1.71185 seconds
    Throughput: 2.92082e+06 messages/sec
    Latency: 342.369 ns/message
    Max queue size: 1095
    Pool size: 2289
    Checksum: 662512377500000

    Test: ScSpPool
    Messages: 5000000
    Elapsed: 1.27169 seconds
    Throughput: 3.93176e+06 messages/sec
    Latency: 254.339 ns/message
    Max queue size: 30323
    Pool size: 65536
    Checksum: 662512377500000

    Done
#endif
    
}


int main([[maybe_unused]] int argc,
         [[maybe_unused]] char** argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    // book_updates_pool::test();
    perf_tests::run();

    return EXIT_SUCCESS;
}
