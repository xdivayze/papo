#include <gtest/gtest.h>
#include "services/memory/memory_manager.hpp"
#include "utils/exception.hpp"

class MemoryManagerTest : public ::testing::Test
{
protected:
    static constexpr std::uint32_t STACK_SIZE = 256;
    Service::MemoryManager *mgr_;

    void SetUp() override
    {
        Service::MemoryManager::MemoryManagerContext ctx{STACK_SIZE};
        mgr_ = new Service::MemoryManager(&ctx);
    }

    void TearDown() override
    {
        delete mgr_;
    }

    void *doAllocateStack(std::uint32_t size_bytes, size_t alignment = alignof(std::max_align_t))
    {
        return mgr_->allocateStack(size_bytes, alignment);
    }
    void doFreeStack() { mgr_->freeStack(); }
    void doFreeStackMemory(void *ptr) { mgr_->freeStackMemory(ptr); }

    void *doAllocateStackUnaligned(std::uint32_t size_bytes)
    {
        return mgr_->allocateStackUnaligned(size_bytes);
    }
    void doFreeStackMemoryUnaligned(void *ptr) { mgr_->freeStackMemoryUnaligned(ptr); }

    Service::StackAllocater::Marker getMarker() { return mgr_->stackAllocater_->getMarker(); }

    void doRecreate(Service::MemoryManager::MemoryManagerContext *ctx)
    {
        delete mgr_;
        mgr_ = new Service::MemoryManager(ctx);
    }
};

// --- aligned allocateStack ---

TEST_F(MemoryManagerTest, AllocateStackReturnsNonNull)
{
    EXPECT_NE(doAllocateStack(16), nullptr);
}

TEST_F(MemoryManagerTest, AllocateStackReturnsAlignedPointer)
{
    constexpr size_t alignment = 16;
    void *ptr = doAllocateStack(16, alignment);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % alignment, 0u);
}

TEST_F(MemoryManagerTest, AllocateStackOverCapacityThrows)
{
    // size + alignment already exceeds STACK_SIZE
    EXPECT_THROW(doAllocateStack(STACK_SIZE), Util::MemoryException);
}

TEST_F(MemoryManagerTest, AllocateStackThatWouldExceedCapacityThrows)
{
    doAllocateStack(100, 16);
    EXPECT_THROW(doAllocateStack(141, 16), Util::MemoryException);
}

TEST_F(MemoryManagerTest, FreeStackResetsMarker)
{
    doAllocateStack(32);
    doFreeStack();
    EXPECT_EQ(getMarker(), 0u);
}

TEST_F(MemoryManagerTest, AllocateAfterFreeStackSucceeds)
{
    doAllocateStack(64, 16);
    doFreeStack();
    EXPECT_NO_THROW(doAllocateStack(64, 16));
}

TEST_F(MemoryManagerTest, FreeStackMemoryRestoresMarkerAfterFirstAlloc)
{
    // Lay down a baseline with unaligned alloc so we can track the marker exactly.
    doAllocateStackUnaligned(32);
    Service::StackAllocater::Marker baseline = getMarker();

    void *ptr = doAllocateStack(16, 16);
    doAllocateStack(8, 8);

    doFreeStackMemory(ptr);

    EXPECT_EQ(getMarker(), baseline + 32u); // baseline + size+alignment of first aligned alloc
}

TEST_F(MemoryManagerTest, FreeStackMemoryNullIsNoOp)
{
    doAllocateStack(16);
    Service::StackAllocater::Marker before = getMarker();
    EXPECT_NO_THROW(doFreeStackMemory(nullptr));
    EXPECT_EQ(getMarker(), before);
}

// --- unaligned allocateStackUnaligned ---

TEST_F(MemoryManagerTest, AllocateStackUnalignedReturnsNonNull)
{
    EXPECT_NE(doAllocateStackUnaligned(16), nullptr);
}

TEST_F(MemoryManagerTest, AllocateStackUnalignedAdvancesMarkerExactly)
{
    doAllocateStackUnaligned(16);
    EXPECT_EQ(getMarker(), 16u);
}

TEST_F(MemoryManagerTest, MultipleUnalignedAllocatesAdvanceMarkerCumulatively)
{
    doAllocateStackUnaligned(10);
    doAllocateStackUnaligned(20);
    EXPECT_EQ(getMarker(), 30u);
}

TEST_F(MemoryManagerTest, AllocateStackUnalignedOverCapacityThrows)
{
    EXPECT_THROW(doAllocateStackUnaligned(STACK_SIZE), Util::MemoryException);
}

TEST_F(MemoryManagerTest, AllocateStackUnalignedThatWouldExceedCapacityThrows)
{
    doAllocateStackUnaligned(200);
    EXPECT_THROW(doAllocateStackUnaligned(57), Util::MemoryException);
}

TEST_F(MemoryManagerTest, FreeStackMemoryUnalignedRestoresMarkerToPointerOffset)
{
    void *ptrA = doAllocateStackUnaligned(32);
    doAllocateStackUnaligned(16);

    doFreeStackMemoryUnaligned(ptrA);

    EXPECT_EQ(getMarker(), 32u);
}

TEST_F(MemoryManagerTest, AllocateAfterFreeStackMemoryUnalignedSucceeds)
{
    void *ptr = doAllocateStackUnaligned(64);
    doAllocateStackUnaligned(32);
    doFreeStackMemoryUnaligned(ptr);
    EXPECT_NO_THROW(doAllocateStackUnaligned(32));
}

// --- construction ---

TEST_F(MemoryManagerTest, NullContextUsesDefaultStackSize)
{
    doRecreate(nullptr);
    EXPECT_NO_THROW(doAllocateStack(1024));
}
