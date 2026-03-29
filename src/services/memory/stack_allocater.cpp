#include "services/memory/stack_allocater.hpp"
#include <cstdlib>
#include <cstddef>
#include <stdexcept>
#include "utils/exception.hpp"
constexpr const char *TAG = "STACK ALLOCATER";

namespace Services
{
    StackAllocater::Marker StackAllocater::getMarker()
    {
        return marker_;
    }

    void *StackAllocater::alloc(std::uint32_t size_bytes)
    {
        if (marker_ + size_bytes >= stackSize_bytes_)
        {
            throw Util::MemoryException(TAG, "allocated memory is out of bounds");
        }

        marker_ = marker_ + size_bytes;
        return stack_ + marker_;
    }

    void StackAllocater::clear()
    {
        marker_ = 0;
    }

    void StackAllocater::freeToMarker(Marker marker)
    {
        if (marker > marker_)
        {
            throw Util::MemoryException(TAG, "free to marker is used as an allocator, ie marker > this.marker");
        }
        marker_ = marker;
    }

    StackAllocater::StackAllocater(std::uint32_t stackSize_bytes)
    {
        stack_ = static_cast<std::byte *>(malloc(stackSize_bytes));
        if (!stack_)
        {

            throw Util::MemoryException(TAG, "memory block couldn't be allocated");
        }

        marker_ = 0;
        stackSize_bytes_ = stackSize_bytes;
    }

    StackAllocater::~StackAllocater()
    {
        free(stack_);
        marker_ = 0;
    }
}