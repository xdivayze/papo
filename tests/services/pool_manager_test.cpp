#include <gtest/gtest.h>
#include "services/memory/pool_manager.hpp"
#include "utils/exception.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

struct Widget
{
    int x;
    float y;
};

// Custom IPool for verifying delegation in registerIPool / releaseToPool.
// Private virtual overrides are dispatched through PoolManager (friend of IPool).
class MockPool : public Memory::IPool
{
public:
    int alloc_count = 0;
    int free_count = 0;
    void *next_alloc;
    size_t cap;

    explicit MockPool(size_t capacity, void *next = nullptr)
        : next_alloc(next), cap(capacity) {}

    size_t capacity() const override { return cap; }

private:
    void *alloc() override
    {
        ++alloc_count;
        return next_alloc;
    }

    void free(void *) override
    {
        ++free_count;
    }
};

class PoolManagerTest : public ::testing::Test
{
protected:
    Memory::PoolManager manager_;
};

// --- hasPool ---

TEST_F(PoolManagerTest, HasPoolReturnsFalseWhenEmpty)
{
    EXPECT_FALSE(manager_.hasPool<int>());
}

TEST_F(PoolManagerTest, HasPoolReturnsTrueAfterRegister)
{
    manager_.registerPool<int>(4);
    EXPECT_TRUE(manager_.hasPool<int>());
}

TEST_F(PoolManagerTest, HasPoolReturnsFalseForUnregisteredType)
{
    manager_.registerPool<int>(4);
    EXPECT_FALSE(manager_.hasPool<Widget>());
}

TEST_F(PoolManagerTest, HasPoolReturnsFalseAfterRemove)
{
    manager_.registerPool<int>(4);
    manager_.removePoolAllocater<int>();
    EXPECT_FALSE(manager_.hasPool<int>());
}

// --- registerPool ---

TEST_F(PoolManagerTest, RegisterPoolCreatesPool)
{
    manager_.registerPool<Widget>(8);
    EXPECT_TRUE(manager_.hasPool<Widget>());
}

TEST_F(PoolManagerTest, RegisterPoolThrowsWhenPoolAlreadyExists)
{
    manager_.registerPool<int>(4);
    EXPECT_THROW(manager_.registerPool<int>(4), Util::LogicException);
}

TEST_F(PoolManagerTest, RegisterPoolAllowsDifferentTypes)
{
    manager_.registerPool<int>(4);
    manager_.registerPool<Widget>(4);
    EXPECT_TRUE(manager_.hasPool<int>());
    EXPECT_TRUE(manager_.hasPool<Widget>());
}

// --- registerIPool ---

TEST_F(PoolManagerTest, RegisterIPoolCreatesPool)
{
    auto mock = std::make_unique<MockPool>(4);
    manager_.registerIPool<Widget>(std::move(mock));
    EXPECT_TRUE(manager_.hasPool<Widget>());
}

TEST_F(PoolManagerTest, RegisterIPoolThrowsWhenPoolAlreadyExists)
{
    manager_.registerPool<Widget>(4);
    auto mock = std::make_unique<MockPool>(4);
    EXPECT_THROW(manager_.registerIPool<Widget>(std::move(mock)), Util::LogicException);
}

TEST_F(PoolManagerTest, RegisterIPoolDelegatesAllocToCustomPool)
{
    static Widget dummy;
    MockPool *mockRaw = new MockPool(1, &dummy);
    manager_.registerIPool<Widget>(std::unique_ptr<Memory::IPool>(mockRaw));

    manager_.acquireFromPool<Widget>();

    EXPECT_EQ(mockRaw->alloc_count, 1);
}

// --- acquireFromPool ---

TEST_F(PoolManagerTest, AcquireReturnsNonNullFromFreshPool)
{
    manager_.registerPool<Widget>(4);
    EXPECT_NE(manager_.acquireFromPool<Widget>(), nullptr);
}

TEST_F(PoolManagerTest, AcquireThrowsWhenPoolNotRegistered)
{
    EXPECT_THROW(manager_.acquireFromPool<Widget>(), Util::LogicException);
}

TEST_F(PoolManagerTest, AcquireReturnsNullWhenPoolExhausted)
{
    manager_.registerPool<Widget>(2);
    manager_.acquireFromPool<Widget>();
    manager_.acquireFromPool<Widget>();
    EXPECT_EQ(manager_.acquireFromPool<Widget>(), nullptr);
}

TEST_F(PoolManagerTest, MultipleAcquiresReturnDistinctPointers)
{
    constexpr size_t CAP = 4;
    manager_.registerPool<Widget>(CAP);
    std::unordered_set<Widget *> ptrs;
    for (size_t i = 0; i < CAP; ++i)
    {
        Widget *p = manager_.acquireFromPool<Widget>();
        ASSERT_NE(p, nullptr) << "alloc failed at index " << i;
        EXPECT_TRUE(ptrs.insert(p).second) << "duplicate pointer at index " << i;
    }
}

TEST_F(PoolManagerTest, AcquireReturnedPointerIsAligned)
{
    manager_.registerPool<Widget>(4);
    Widget *p = manager_.acquireFromPool<Widget>();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % alignof(Widget), 0u);
}

// --- releaseToPool ---

TEST_F(PoolManagerTest, ReleaseThrowsWhenPoolNotRegistered)
{
    Widget *dummy = nullptr;
    EXPECT_THROW(manager_.releaseToPool(dummy), Util::LogicException);
}

TEST_F(PoolManagerTest, ReleaseDelegatesFreeToUnderlyingPool)
{
    static Widget dummy;
    MockPool *mockRaw = new MockPool(1, &dummy);
    manager_.registerIPool<Widget>(std::unique_ptr<Memory::IPool>(mockRaw));

    Widget *p = manager_.acquireFromPool<Widget>();
    manager_.releaseToPool(p);

    EXPECT_EQ(mockRaw->free_count, 1);
}

TEST_F(PoolManagerTest, ReleaseAllowsReacquireAfterExhaustion)
{
    manager_.registerPool<Widget>(1);
    Widget *p = manager_.acquireFromPool<Widget>();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(manager_.acquireFromPool<Widget>(), nullptr);

    manager_.releaseToPool(p);
    EXPECT_NE(manager_.acquireFromPool<Widget>(), nullptr);
}

TEST_F(PoolManagerTest, ExhaustFreeAllReacquireAll)
{
    constexpr size_t CAP = 4;
    manager_.registerPool<Widget>(CAP);

    Widget *ptrs[CAP];
    for (size_t i = 0; i < CAP; ++i)
        ptrs[i] = manager_.acquireFromPool<Widget>();

    ASSERT_EQ(manager_.acquireFromPool<Widget>(), nullptr);

    for (size_t i = 0; i < CAP; ++i)
        manager_.releaseToPool(ptrs[i]);

    for (size_t i = 0; i < CAP; ++i)
        EXPECT_NE(manager_.acquireFromPool<Widget>(), nullptr) << "reacquire failed at index " << i;
}

// --- removePoolAllocater ---

TEST_F(PoolManagerTest, RemovePoolSucceeds)
{
    manager_.registerPool<int>(4);
    ASSERT_NO_THROW(manager_.removePoolAllocater<int>());
    EXPECT_FALSE(manager_.hasPool<int>());
}

TEST_F(PoolManagerTest, RemovePoolThrowsWhenNotRegistered)
{
    EXPECT_THROW(manager_.removePoolAllocater<int>(), Util::LogicException);
}

TEST_F(PoolManagerTest, RemovePoolDoesNotAffectOtherPools)
{
    manager_.registerPool<int>(4);
    manager_.registerPool<Widget>(4);
    manager_.removePoolAllocater<int>();
    EXPECT_FALSE(manager_.hasPool<int>());
    EXPECT_TRUE(manager_.hasPool<Widget>());
}

TEST_F(PoolManagerTest, ReregisterAfterRemoveSucceeds)
{
    manager_.registerPool<int>(4);
    manager_.removePoolAllocater<int>();
    ASSERT_NO_THROW(manager_.registerPool<int>(8));
    EXPECT_TRUE(manager_.hasPool<int>());
}

// --- concurrency (synchronized PoolManager) ---

// Many threads hammer acquire/release on the SAME pool. Invariants:
//  - a chunk handed out is never simultaneously owned by two threads
//  - the free-list survives intact: exactly CAP distinct chunks remain
TEST_F(PoolManagerTest, ConcurrentSamePoolAcquireRelease)
{
    constexpr size_t CAP = 64;
    constexpr int THREADS = 8;
    constexpr int ITERS = 4000;
    manager_.registerPool<Widget>(CAP);

    std::mutex liveMutex;
    std::unordered_set<Widget *> live; // chunks currently held by some thread
    std::atomic<bool> doubleHandout{false};

    auto worker = [&]
    {
        for (int i = 0; i < ITERS; ++i)
        {
            Widget *p = manager_.acquireFromPool<Widget>();
            if (!p)
                continue; // pool momentarily exhausted by peers — fine
            {
                std::lock_guard<std::mutex> lg(liveMutex);
                if (!live.insert(p).second)
                    doubleHandout = true; // same chunk given to two threads
            }
            p->x = i; // touch the memory
            {
                std::lock_guard<std::mutex> lg(liveMutex);
                live.erase(p);
            }
            manager_.releaseToPool(p);
        }
    };

    std::vector<std::thread> ts;
    for (int t = 0; t < THREADS; ++t)
        ts.emplace_back(worker);
    for (auto &t : ts)
        t.join();

    EXPECT_FALSE(doubleHandout.load());
    EXPECT_TRUE(live.empty());

    // Free-list integrity: all CAP chunks are re-acquirable and distinct.
    std::unordered_set<Widget *> drained;
    for (size_t i = 0; i < CAP; ++i)
    {
        Widget *p = manager_.acquireFromPool<Widget>();
        ASSERT_NE(p, nullptr) << "lost a chunk at index " << i;
        EXPECT_TRUE(drained.insert(p).second) << "duplicate chunk at " << i;
    }
    EXPECT_EQ(manager_.acquireFromPool<Widget>(), nullptr); // exactly CAP
}

// Distinct-type pools used in parallel must not corrupt one another
// (per-pool mutex => cross-type parallelism with no shared state).
TEST_F(PoolManagerTest, ConcurrentDistinctPoolsAreIndependent)
{
    constexpr size_t CAP = 32;
    constexpr int ITERS = 4000;
    manager_.registerPool<int>(CAP);
    manager_.registerPool<Widget>(CAP);

    std::atomic<bool> failure{false};

    auto hammer = [&](auto tag)
    {
        using T = typename decltype(tag)::type;
        for (int i = 0; i < ITERS; ++i)
        {
            T *p = manager_.acquireFromPool<T>();
            if (!p)
                continue;
            manager_.releaseToPool(p);
        }
    };
    struct IntTag { using type = int; };
    struct WidgetTag { using type = Widget; };

    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] { hammer(IntTag{}); });
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] { hammer(WidgetTag{}); });
    for (auto &t : ts)
        t.join();

    EXPECT_FALSE(failure.load());
    // Both pools fully recovered.
    std::unordered_set<int *> ints;
    for (size_t i = 0; i < CAP; ++i)
    {
        int *p = manager_.acquireFromPool<int>();
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(ints.insert(p).second);
    }
    std::unordered_set<Widget *> widgets;
    for (size_t i = 0; i < CAP; ++i)
    {
        Widget *p = manager_.acquireFromPool<Widget>();
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(widgets.insert(p).second);
    }
}
