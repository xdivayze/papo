#include <gtest/gtest.h>
#include "services/memory/pool_manager.hpp"
#include "utils/exception.hpp"
#include <unordered_set>

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
    EXPECT_THROW(manager_.registerPool<int>(4), Util::LogicException *);
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
    EXPECT_THROW(manager_.registerIPool<Widget>(std::move(mock)), Util::LogicException *);
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
    EXPECT_THROW(manager_.acquireFromPool<Widget>(), Util::LogicException *);
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
    EXPECT_THROW(manager_.releaseToPool(dummy), Util::LogicException *);
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
    EXPECT_THROW(manager_.removePoolAllocater<int>(), Util::LogicException *);
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
