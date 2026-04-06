#include <gtest/gtest.h>
#include "services/memory/arena_allocator.hpp"

class DoubleBufferedArenaTest : public ::testing::Test
{
protected:
    static constexpr size_t CAPACITY = 256;
    Memory::DoubleBufferedArena arena{CAPACITY};
};

// --- buffer identity ---

TEST_F(DoubleBufferedArenaTest, WriteAndReadBuffersAreDifferent)
{
    EXPECT_NE(arena.writeBuffer(), arena.readBuffer());
}

TEST_F(DoubleBufferedArenaTest, WriteBufferIsNonNull)
{
    EXPECT_NE(arena.writeBuffer(), nullptr);
}

TEST_F(DoubleBufferedArenaTest, ReadBufferIsNonNull)
{
    EXPECT_NE(arena.readBuffer(), nullptr);
}

// --- swapBuffers ---

TEST_F(DoubleBufferedArenaTest, SwapExchangesWriteAndReadBuffers)
{
    std::byte *prevWrite = arena.writeBuffer();
    std::byte *prevRead  = arena.readBuffer();

    arena.swapBuffers();

    EXPECT_EQ(arena.writeBuffer(), prevRead);
    EXPECT_EQ(arena.readBuffer(),  prevWrite);
}

TEST_F(DoubleBufferedArenaTest, DoubleSwapRestoresOriginalBuffers)
{
    std::byte *origWrite = arena.writeBuffer();
    std::byte *origRead  = arena.readBuffer();

    arena.swapBuffers();
    arena.swapBuffers();

    EXPECT_EQ(arena.writeBuffer(), origWrite);
    EXPECT_EQ(arena.readBuffer(),  origRead);
}

TEST_F(DoubleBufferedArenaTest, SwapResetsNewWriteBufferOffset)
{
    // Fill the write buffer, swap, then verify the new write buffer accepts allocations from scratch
    (void)arena.allocate(CAPACITY - 8, 1);  // nearly fill it
    arena.swapBuffers();

    // After swap, the new write buffer should be reset and accept a full allocation
    EXPECT_NE(arena.allocate(CAPACITY - 8, 1), nullptr);
}

// --- do_allocate (via std::pmr::memory_resource::allocate) ---

TEST_F(DoubleBufferedArenaTest, AllocateReturnsNonNull)
{
    EXPECT_NE(arena.allocate(16, 1), nullptr);
}

TEST_F(DoubleBufferedArenaTest, AllocateReturnsPointerInsideWriteBuffer)
{
    std::byte *base = arena.writeBuffer();
    void *ptr = arena.allocate(16, 1);
    EXPECT_GE(reinterpret_cast<std::byte *>(ptr), base);
    EXPECT_LT(reinterpret_cast<std::byte *>(ptr), base + CAPACITY);
}

TEST_F(DoubleBufferedArenaTest, MultipleAllocationsReturnDistinctPointers)
{
    void *a = arena.allocate(16, 1);
    void *b = arena.allocate(16, 1);
    EXPECT_NE(a, b);
}

TEST_F(DoubleBufferedArenaTest, SequentialAllocationsDoNotOverlap)
{
    constexpr size_t SZ = 32;
    auto *a = static_cast<std::byte *>(arena.allocate(SZ, 1));
    auto *b = static_cast<std::byte *>(arena.allocate(SZ, 1));
    // b must start at or after the end of a
    EXPECT_GE(b, a + SZ);
}

TEST_F(DoubleBufferedArenaTest, AllocationsGoToWriteBufferNotReadBuffer)
{
    std::byte *readBase = arena.readBuffer();
    void *ptr = arena.allocate(16, 1);
    auto *p = static_cast<std::byte *>(ptr);
    // pointer must NOT be inside the read buffer
    EXPECT_FALSE(p >= readBase && p < readBase + CAPACITY);
}

// --- alignment ---

TEST_F(DoubleBufferedArenaTest, AllocateRespectsAlignment4)
{
    void *ptr = arena.allocate(1, 4);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % 4, 0u);
}

TEST_F(DoubleBufferedArenaTest, AllocateRespectsAlignment8)
{
    void *ptr = arena.allocate(1, 8);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % 8, 0u);
}

TEST_F(DoubleBufferedArenaTest, AllocateRespectsAlignment16)
{
    void *ptr = arena.allocate(1, 16);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % 16, 0u);
}

TEST_F(DoubleBufferedArenaTest, AllocateRespectsAlignmentAfterUnalignedAlloc)
{
    (void)arena.allocate(3, 1);  // leave offset at 3
    void *ptr = arena.allocate(8, 8);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % 8, 0u);
}

// --- overflow ---

TEST_F(DoubleBufferedArenaTest, OverflowTriggersAssert)
{
    EXPECT_DEATH((void)arena.allocate(CAPACITY + 1, 1), "");
}

// --- swap resets write buffer (post-allocation state persists in read buffer) ---

TEST_F(DoubleBufferedArenaTest, DataWrittenBeforeSwapIsAccessibleViaReadBufferAfterSwap)
{
    auto *writeBase = static_cast<std::byte *>(arena.allocate(8, 1));
    writeBase[0] = std::byte{0xAB};

    std::byte *prevWrite = arena.writeBuffer();
    arena.swapBuffers();

    // The old write buffer is now the read buffer
    EXPECT_EQ(arena.readBuffer()[0], std::byte{0xAB});
    EXPECT_EQ(arena.readBuffer(), prevWrite);
}

// --- do_is_equal ---

TEST_F(DoubleBufferedArenaTest, IsEqualToItself)
{
    EXPECT_TRUE(arena.is_equal(arena));
}

TEST_F(DoubleBufferedArenaTest, IsNotEqualToDifferentArena)
{
    Memory::DoubleBufferedArena other{CAPACITY};
    EXPECT_FALSE(arena.is_equal(other));
}
