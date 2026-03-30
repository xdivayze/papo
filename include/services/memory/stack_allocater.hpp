#pragma once
#include <cstdint>
#include <cstddef>
class StackAllocaterTest;
namespace Services
{
    class MemoryManager;
    class StackAllocater
    {
    public:
        friend class MemoryManager;
        friend class ::StackAllocaterTest;
        typedef std::uint32_t Marker;
        Marker getMarker();

    private:
        void *alloc(std::uint32_t size_bytes);
        void freeToMarker(Marker marker);
        void clear();
        explicit StackAllocater(std::uint32_t stackSize_bytes);
        ~StackAllocater();
        std::uint32_t stackSize_bytes_;
        std::byte *stack_;
        Marker marker_;
    };
}