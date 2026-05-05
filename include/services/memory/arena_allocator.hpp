#pragma once
#include <cstddef>
#include <memory_resource>
#include <cassert>
namespace Memory
{

    class DoubleBufferedArena : public std::pmr::memory_resource
    {

    public:
        std::byte *writeBuffer();
        std::byte *readBuffer();
        void swapBuffers();

        DoubleBufferedArena(size_t capacity);
        ~DoubleBufferedArena();

    protected:
        struct Arena
        {
            std::byte *buffer_;
            size_t capacity_;
            size_t offset_ = 0;

            void *allocate(size_t size, size_t alignment) 
            {
                offset_ = (offset_ + alignment - 1) & ~(alignment - 1);
                void *ptr = buffer_ + offset_;
                offset_ += size;
                assert(offset_ <= capacity_);
                return ptr;
            }

            void reset() { offset_ = 0; }
        };

        void *do_allocate(size_t size_bytes, size_t alignment) override;

        void do_deallocate(void *p, size_t bytes, size_t alignment_) override {};

        bool do_is_equal(const memory_resource &other) const noexcept override;

    private:
        Arena buffers_[2];
        int write_index_ = 0;
    };

}