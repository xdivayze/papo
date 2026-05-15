#pragma once
#include <cstdint>
#include <cstddef>
class StackAllocaterTest;
class ModelTest;
namespace Service
{
    class MemoryManager;
    class Model;
    class AssetManager;
    class StackAllocater
    {
    public:
        friend class MemoryManager;
        friend class Model;
        friend class AssetManager;
        friend class ::StackAllocaterTest;
        friend class ::ModelTest;

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