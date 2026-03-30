#include <gtest/gtest.h>
#include "services/memory/stack_allocater.hpp"
#include "utils/exception.hpp"

class StackAllocaterTest : public ::testing::Test
{
protected:
    static constexpr std::uint32_t STACK_SIZE = 128;
    Services::StackAllocater alloc{STACK_SIZE};

    void *doAlloc(std::uint32_t size_bytes) { return alloc.alloc(size_bytes); }
    void doClear() { alloc.clear(); }
    void doFreeToMarker(Services::StackAllocater::Marker marker) { alloc.freeToMarker(marker); }
};

TEST_F(StackAllocaterTest, InitialMarkerIsZero)
{
    EXPECT_EQ(alloc.getMarker(), 0u);
}

TEST_F(StackAllocaterTest, AllocReturnsNonNull)
{
    void *ptr = doAlloc(16);
    EXPECT_NE(ptr, nullptr);
}

TEST_F(StackAllocaterTest, AllocAdvancesMarker)
{
    doAlloc(16);
    EXPECT_EQ(alloc.getMarker(), 16u);
}

TEST_F(StackAllocaterTest, MultipleAllocsAdvanceMarkerCumulatively)
{
    doAlloc(10);
    doAlloc(20);
    EXPECT_EQ(alloc.getMarker(), 30u);
}

TEST_F(StackAllocaterTest, AllocExactCapacityThrows)
{
    EXPECT_THROW(doAlloc(STACK_SIZE), Util::MemoryException);
}

TEST_F(StackAllocaterTest, AllocOverCapacityThrows)
{
    EXPECT_THROW(doAlloc(STACK_SIZE + 1), Util::MemoryException);
}

TEST_F(StackAllocaterTest, AllocThatWouldExceedCapacityThrows)
{
    doAlloc(64);
    EXPECT_THROW(doAlloc(65), Util::MemoryException);
}

TEST_F(StackAllocaterTest, ClearResetsMarkerToZero)
{
    doAlloc(32);
    doClear();
    EXPECT_EQ(alloc.getMarker(), 0u);
}

TEST_F(StackAllocaterTest, AllocAfterClearSucceeds)
{
    doAlloc(64);
    doClear();
    EXPECT_NO_THROW(doAlloc(64));
}

TEST_F(StackAllocaterTest, FreeToMarkerRestoresPreviousMarker)
{
    Services::StackAllocater::Marker before = alloc.getMarker();
    doAlloc(32);
    doFreeToMarker(before);
    EXPECT_EQ(alloc.getMarker(), before);
}

TEST_F(StackAllocaterTest, FreeToCurrentMarkerIsNoOp)
{
    doAlloc(32);
    Services::StackAllocater::Marker current = alloc.getMarker();
    doFreeToMarker(current);
    EXPECT_EQ(alloc.getMarker(), current);
}

TEST_F(StackAllocaterTest, FreeToMarkerAheadOfCurrentThrows)
{
    doAlloc(16);
    Services::StackAllocater::Marker ahead = alloc.getMarker() + 8;
    EXPECT_THROW(doFreeToMarker(ahead), Util::MemoryException);
}

TEST_F(StackAllocaterTest, FreeToMarkerThrowIsCatchableAsMemoryException)
{
    doAlloc(16);
    Services::StackAllocater::Marker ahead = alloc.getMarker() + 1;
    EXPECT_THROW(
        {
            try { doFreeToMarker(ahead); }
            catch (const Util::MemoryException &) { throw; }
        },
        Util::MemoryException);
}

TEST_F(StackAllocaterTest, GetMarkerReturnsConsistentValueWithoutAlloc)
{
    Services::StackAllocater::Marker m1 = alloc.getMarker();
    Services::StackAllocater::Marker m2 = alloc.getMarker();
    EXPECT_EQ(m1, m2);
}
