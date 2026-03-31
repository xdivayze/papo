#include <gtest/gtest.h>
#include "services/memory/pool_allocater.hpp"
#include <unordered_set>

struct alignas(16) Aligned16
{
    int x;
};

class PoolAllocaterTest : public ::testing::Test
{
protected:
    static constexpr size_t CAPACITY = 4;
    Memory::PoolAllocater<int> *pool_;

    void SetUp() override
    {
        pool_ = new Memory::PoolAllocater<int>(CAPACITY);
    }

    void TearDown() override
    {
        delete pool_;
    }

    // --- helpers for the default int pool ---
    void *doAlloc() { return pool_->alloc(); }
    void doFree(void *ptr) { pool_->free(ptr); }

    // --- helpers for ad-hoc int pools ---
    Memory::PoolAllocater<int> *newIntPool(size_t cap) { return new Memory::PoolAllocater<int>(cap); }
    void deleteIntPool(Memory::PoolAllocater<int> *p) { delete p; }
    void *allocFrom(Memory::PoolAllocater<int> *p) { return p->alloc(); }
    void freeFrom(Memory::PoolAllocater<int> *p, void *ptr) { p->free(ptr); }

    // --- helpers for Aligned16 pools ---
    Memory::PoolAllocater<Aligned16> *newAligned16Pool(size_t cap) { return new Memory::PoolAllocater<Aligned16>(cap); }
    void deleteAligned16Pool(Memory::PoolAllocater<Aligned16> *p) { delete p; }
    void *allocFrom(Memory::PoolAllocater<Aligned16> *p) { return p->alloc(); }
};

// --- capacity ---

TEST_F(PoolAllocaterTest, CapacityMatchesConstructedSize)
{
    EXPECT_EQ(pool_->capacity(), CAPACITY);
}

// --- alloc ---

TEST_F(PoolAllocaterTest, AllocReturnsNonNullWhenNotExhausted)
{
    EXPECT_NE(doAlloc(), nullptr);
}

TEST_F(PoolAllocaterTest, MultipleAllocsReturnDistinctPointers)
{
    std::unordered_set<void *> ptrs;
    for (size_t i = 0; i < CAPACITY; ++i)
    {
        void *ptr = doAlloc();
        ASSERT_NE(ptr, nullptr);
        EXPECT_TRUE(ptrs.insert(ptr).second) << "duplicate pointer at index " << i;
    }
}

TEST_F(PoolAllocaterTest, AllocReturnsNullWhenExhausted)
{
    for (size_t i = 0; i < CAPACITY; ++i)
        doAlloc();

    EXPECT_EQ(doAlloc(), nullptr);
}

TEST_F(PoolAllocaterTest, AllocReturnedPointerIsAlignedForInt)
{
    void *ptr = doAlloc();
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % alignof(int), 0u);
}

// --- free ---

TEST_F(PoolAllocaterTest, FreeAllowsReallocWhenExhausted)
{
    void *saved = doAlloc();
    for (size_t i = 1; i < CAPACITY; ++i)
        doAlloc();

    ASSERT_EQ(doAlloc(), nullptr);

    doFree(saved);
    EXPECT_NE(doAlloc(), nullptr);
}

TEST_F(PoolAllocaterTest, FreeThenAllocReturnsSamePointerLIFO)
{
    void *ptr = doAlloc();
    doFree(ptr);
    EXPECT_EQ(doAlloc(), ptr);
}

TEST_F(PoolAllocaterTest, FreeChunksAreReturnedInLIFOOrder)
{
    void *first = doAlloc();
    void *second = doAlloc();

    doFree(first);
    doFree(second);

    EXPECT_EQ(doAlloc(), second);
    EXPECT_EQ(doAlloc(), first);
}

TEST_F(PoolAllocaterTest, ExhaustFreeAllThenExhaustAgain)
{
    void *ptrs[CAPACITY];
    for (size_t i = 0; i < CAPACITY; ++i)
        ptrs[i] = doAlloc();

    ASSERT_EQ(doAlloc(), nullptr);

    for (size_t i = 0; i < CAPACITY; ++i)
        doFree(ptrs[i]);

    for (size_t i = 0; i < CAPACITY; ++i)
        EXPECT_NE(doAlloc(), nullptr) << "alloc failed at index " << i << " after full free";
}

// --- capacity of 1 ---

TEST_F(PoolAllocaterTest, CapacityOneAllocAndFreeWorks)
{
    Memory::PoolAllocater<int> *p = newIntPool(1);

    void *ptr = allocFrom(p);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(allocFrom(p), nullptr);

    freeFrom(p, ptr);
    EXPECT_EQ(allocFrom(p), ptr);

    deleteIntPool(p);
}

// --- alignment with over-aligned type ---

TEST_F(PoolAllocaterTest, AllocReturnsCorrectlyAlignedPointerForAligned16)
{
    Memory::PoolAllocater<Aligned16> *p = newAligned16Pool(4);

    for (int i = 0; i < 4; ++i)
    {
        void *ptr = allocFrom(p);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % 16, 0u) << "misaligned at index " << i;
    }

    deleteAligned16Pool(p);
}
