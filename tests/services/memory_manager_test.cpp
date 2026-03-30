#include <gtest/gtest.h>
#include "services/memory/memory_manager.hpp"
#include "utils/exception.hpp"

class MemoryManagerTest : public ::testing::Test
{
protected:
    static constexpr std::uint32_t STACK_SIZE = 128;
    Services::MemoryManager *mgr_;

    void SetUp() override
    {
        Services::MemoryManager::MemoryManagerContext ctx{STACK_SIZE};
        mgr_ = new Services::MemoryManager(&ctx);
    }

    void TearDown() override
    {
        delete mgr_;
    }

    void *doAllocateStack(std::uint32_t size_bytes) { return mgr_->allocateStack(size_bytes); }
    void doFreeStack() { mgr_->freeStack(); }
    void doFreeStackToMarker(Services::StackAllocater::Marker marker) { mgr_->freeStackToMarker(marker); }
    Services::StackAllocater::Marker getMarker() { return mgr_->stackAllocater_->getMarker(); }
    void doRecreate(Services::MemoryManager::MemoryManagerContext *ctx)
    {
        delete mgr_;
        mgr_ = new Services::MemoryManager(ctx);
    }
};

TEST_F(MemoryManagerTest, AllocateStackReturnsNonNull)
{
    void *ptr = doAllocateStack(16);
    EXPECT_NE(ptr, nullptr);
}

TEST_F(MemoryManagerTest, AllocateStackAdvancesMarker)
{
    doAllocateStack(16);
    EXPECT_EQ(getMarker(), 16u);
}

TEST_F(MemoryManagerTest, MultipleAllocatesAdvanceMarkerCumulatively)
{
    doAllocateStack(10);
    doAllocateStack(20);
    EXPECT_EQ(getMarker(), 30u);
}

TEST_F(MemoryManagerTest, AllocateStackOverCapacityThrows)
{
    EXPECT_THROW(doAllocateStack(STACK_SIZE), Util::MemoryException);
}

TEST_F(MemoryManagerTest, AllocateStackThatWouldExceedCapacityThrows)
{
    doAllocateStack(64);
    EXPECT_THROW(doAllocateStack(65), Util::MemoryException);
}

TEST_F(MemoryManagerTest, FreeStackResetsMarker)
{
    doAllocateStack(32);
    doFreeStack();
    EXPECT_EQ(getMarker(), 0u);
}

TEST_F(MemoryManagerTest, AllocateAfterFreeStackSucceeds)
{
    doAllocateStack(64);
    doFreeStack();
    EXPECT_NO_THROW(doAllocateStack(64));
}

TEST_F(MemoryManagerTest, FreeStackToMarkerRestoresMarker)
{
    Services::StackAllocater::Marker before = getMarker();
    doAllocateStack(32);
    doFreeStackToMarker(before);
    EXPECT_EQ(getMarker(), before);
}

TEST_F(MemoryManagerTest, FreeStackToCurrentMarkerIsNoOp)
{
    doAllocateStack(32);
    Services::StackAllocater::Marker current = getMarker();
    doFreeStackToMarker(current);
    EXPECT_EQ(getMarker(), current);
}

TEST_F(MemoryManagerTest, FreeStackToMarkerAheadOfCurrentThrows)
{
    doAllocateStack(16);
    Services::StackAllocater::Marker ahead = getMarker() + 8;
    EXPECT_THROW(doFreeStackToMarker(ahead), Util::MemoryException);
}

TEST_F(MemoryManagerTest, NullContextUsesDefaultStackSize)
{
    doRecreate(nullptr);
    EXPECT_NO_THROW(doAllocateStack(1024));
}
