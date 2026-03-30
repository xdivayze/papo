#pragma once
#include "../../utils/singleton.hpp"
#include <cstdint>
#include "stack_allocater.hpp"
class MemoryManagerTest;
namespace Engine { class Root; }
namespace Services
{
    class MemoryManager
    {
    public:
        typedef struct
        {
            std::uint32_t StackSize;
        } MemoryManagerContext;

        friend class Engine::Root;
        friend class ::MemoryManagerTest;
        void *allocateStack(std::uint32_t size_bytes);
        void freeStackToMarker(StackAllocater::Marker marker);
        void freeStack();

    private:
        StackAllocater *stackAllocater_;
        explicit MemoryManager(MemoryManagerContext *ctx);
        ~MemoryManager();
    };
}