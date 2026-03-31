#include <gtest/gtest.h>
#include "services/memory/stack_allocater.hpp"
#include "utils/exception.hpp"

class StackAllocaterTest : public ::testing::Test
{
protected:
    static constexpr std::uint32_t STACK_SIZE = 256;
    Services::StackAllocater alloc{STACK_SIZE};

    void *doAlloc(std::uint32_t size_bytes) { return alloc.alloc(size_bytes); }
    void doClear() { alloc.clear(); }
    void doFreeToMarker(Services::StackAllocater::Marker marker) { alloc.freeToMarker(marker); }
    void *doAllocAligned(std::uint32_t size_bytes, size_t alignment) { return alloc.allocAligned(size_bytes, alignment); }
    void doFreeAligned(void *ptr) { alloc.freeAligned(ptr); }
    void doFreeUnaligned(void *ptr) { alloc.freeUnaligned(ptr); }
    Services::StackAllocater::Marker doPointerToMarker(void *ptr) { return alloc.pointerToMarker(ptr); }
};

// --- unaligned alloc ---

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
    doAlloc(128);
    EXPECT_THROW(doAlloc(129), Util::MemoryException);
}

TEST_F(StackAllocaterTest, ClearResetsMarkerToZero)
{
    doAlloc(32);
    doClear();
    EXPECT_EQ(alloc.getMarker(), 0u);
}

TEST_F(StackAllocaterTest, AllocAfterClearSucceeds)
{
    doAlloc(128);
    doClear();
    EXPECT_NO_THROW(doAlloc(128));
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

// --- pointerToMarker ---

TEST_F(StackAllocaterTest, PointerToMarkerReturnsOffsetOfAllocatedPointer)
{
    void *ptr = doAlloc(32);
    EXPECT_EQ(doPointerToMarker(ptr), 32u);
}

TEST_F(StackAllocaterTest, PointerToMarkerAfterMultipleAllocsReturnsCorrectOffset)
{
    doAlloc(16);
    void *ptr = doAlloc(24);
    EXPECT_EQ(doPointerToMarker(ptr), 40u);
}

TEST_F(StackAllocaterTest, PointerToMarkerOutOfBoundsThrows)
{
    int local = 0;
    EXPECT_THROW(doPointerToMarker(&local), Util::MemoryException);
}

// --- aligned alloc / free ---

TEST_F(StackAllocaterTest, AllocAlignedReturnsNonNull)
{
    EXPECT_NE(doAllocAligned(16, 16), nullptr);
}

TEST_F(StackAllocaterTest, AllocAlignedReturnsAlignedPointer)
{
    void *ptr = doAllocAligned(16, 16);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % 16, 0u);
}

TEST_F(StackAllocaterTest, AllocAlignedWithAlignment8ReturnsAlignedPointer)
{
    void *ptr = doAllocAligned(20, 8);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % 8, 0u);
}

TEST_F(StackAllocaterTest, AllocAlignedAdvancesMarkerBySizePlusAlignment)
{
    doAllocAligned(16, 16);
    EXPECT_EQ(alloc.getMarker(), 32u);
}

TEST_F(StackAllocaterTest, AllocAlignedThatWouldExceedCapacityThrows)
{
    // size + alignment = STACK_SIZE overflows
    EXPECT_THROW(doAllocAligned(STACK_SIZE, 16), Util::MemoryException);
}

TEST_F(StackAllocaterTest, FreeAlignedNullIsNoOp)
{
    doAllocAligned(16, 16);
    Services::StackAllocater::Marker before = alloc.getMarker();
    EXPECT_NO_THROW(doFreeAligned(nullptr));
    EXPECT_EQ(alloc.getMarker(), before);
}

TEST_F(StackAllocaterTest, FreeAlignedFirstPtrFreesSubsequentAllocations)
{
    void *ptr1 = doAllocAligned(16, 16);
    Services::StackAllocater::Marker afterFirst = alloc.getMarker();
    doAllocAligned(8, 8);

    doFreeAligned(ptr1);

    EXPECT_EQ(alloc.getMarker(), afterFirst);
}

TEST_F(StackAllocaterTest, AllocAfterFreeAlignedSucceeds)
{
    void *ptr = doAllocAligned(16, 16);
    doAllocAligned(8, 8);
    doFreeAligned(ptr);
    EXPECT_NO_THROW(doAllocAligned(8, 8));
}

// --- freeUnaligned ---

TEST_F(StackAllocaterTest, FreeUnalignedRestoresMarkerToPointerOffset)
{
    void *ptrA = doAlloc(32);
    doAlloc(16);

    doFreeUnaligned(ptrA);

    EXPECT_EQ(alloc.getMarker(), 32u);
}

TEST_F(StackAllocaterTest, FreeUnalignedOnLatestPtrIsNoOp)
{
    doAlloc(16);
    void *ptr = doAlloc(24);
    Services::StackAllocater::Marker current = alloc.getMarker();

    doFreeUnaligned(ptr);

    EXPECT_EQ(alloc.getMarker(), current);
}
