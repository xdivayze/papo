#include "services/memory/arena_allocator.hpp"

namespace Memory
{
    std::byte *DoubleBufferedArena::writeBuffer()
    {
        return buffers_[write_index_].buffer_;
    }

    std::byte *DoubleBufferedArena::readBuffer()
    {
        return buffers_[1 - write_index_].buffer_;
    }

    void DoubleBufferedArena::swapBuffers()
    {
        write_index_ = 1 - write_index_;
        buffers_[write_index_].reset();
    }

    DoubleBufferedArena::DoubleBufferedArena(size_t capacity)
    {
        for (auto &a : buffers_)
        {
            a.buffer_ = new std::byte[capacity];
            a.capacity_ = capacity;
        }
    }
    void *DoubleBufferedArena::do_allocate(size_t size_bytes, size_t alignment)
    {

        return buffers_[write_index_].allocate(size_bytes, alignment);
    }

    bool DoubleBufferedArena::do_is_equal(const std::pmr::memory_resource &other) const noexcept 
    {

        return this == &other;
    }

    DoubleBufferedArena::~DoubleBufferedArena()
    {
        for (auto &a : buffers_)
            delete[] a.buffer_;
    }
}