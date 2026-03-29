#pragma once
#include <cstdint>
namespace Services
{
    class StackAllocater
    {
    public:
        friend class MemoryManager;
        typedef std::uint32_t Marker;
        Marker getMarker();
        void *alloc(std::uint32_t size_bytes);
        void freeToMarker(Marker marker);
        void clear();

    private:
        explicit StackAllocater(std::uint32_t stackSize_bytes);
        ~StackAllocater();
        void *block_;
        Marker marker_;
    };
}