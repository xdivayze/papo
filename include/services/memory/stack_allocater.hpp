#pragma once
#include <cstdint>
#include <cstddef>
class StackAllocaterTest;
namespace Service
{
    class MemoryManager;
    class StackAllocater
    {
    public:
        friend class MemoryManager;
        friend class ::StackAllocaterTest;

        typedef std::uint32_t Marker;
        
        Marker getMarker();

        Marker pointerToMarker(void* ptr);

    private:
        std::uint32_t stackSize_bytes_;
        std::byte *stack_;
        Marker marker_;

        void *allocAligned(std::uint32_t size_bytes, size_t alignment);
        void freeAligned(void *ptr);

        void *alloc(std::uint32_t size_bytes);
        void freeUnaligned(void*ptr);

        void freeToMarker(Marker marker);
        void clear();

        explicit StackAllocater(std::uint32_t stackSize_bytes);
        ~StackAllocater();
    };
}