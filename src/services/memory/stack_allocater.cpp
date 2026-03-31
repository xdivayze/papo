#include "services/memory/stack_allocater.hpp"
#include <cstdlib>
#include <cstddef>
#include <stdexcept>
#include "utils/exception.hpp"
#include "utils/memory_utils.hpp"
#include "utils/exception.hpp"
constexpr const char *TAG = "Stack Allocater";

namespace Services
{
    StackAllocater::Marker StackAllocater::pointerToMarker(void *ptr)
    {
        std::uintptr_t baseAddr = reinterpret_cast<std::uintptr_t>(stack_);
        std::uintptr_t ptrAddr = reinterpret_cast<std::uintptr_t>(ptr);
        std::ptrdiff_t diff = ptrAddr - baseAddr;
        if (diff < 0 || diff > stackSize_bytes_)
        {
            throw Util::MemoryException(TAG, "passed pointer is out of stack's bounds");
        }

        return static_cast<Marker>(diff);
    }
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
    void *StackAllocater::allocAligned(std::uint32_t size_bytes, size_t alignment)
    {
        size_t actualBytes = size_bytes + alignment;
        std::byte *pRawMem = reinterpret_cast<std::byte *>(alloc(actualBytes));

        std::byte *pAlignedMem = Util::AlignPointer(pRawMem, alignment);
        if (pAlignedMem == pRawMem)
        {
            pAlignedMem += alignment;
        }

        ptrdiff_t shift = pAlignedMem - pRawMem;
        if (shift <= 0 || shift > 256)
            throw Util::MemoryException(TAG, "Greater than 1 byte or negative shift detected");
        pAlignedMem[-1] = static_cast<std::byte>(shift);
        return pAlignedMem;
    }

    void StackAllocater::freeAligned(void *ptr)
    {
        if (!ptr)
            return;

        std::uint8_t *pAlignedMem = reinterpret_cast<std::uint8_t *>(ptr);
        ptrdiff_t shift = pAlignedMem[-1];
        if (shift == 0)
            shift = 256;

        uint8_t *pRawMem = pAlignedMem - shift;
        freeToMarker(pointerToMarker(pRawMem));
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
        ::free(stack_);
    }

}