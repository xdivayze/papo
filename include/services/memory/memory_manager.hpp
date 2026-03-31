#pragma once
#include "../../utils/singleton.hpp"
#include <cstdint>
#include "stack_allocater.hpp"
#include <cstddef>
class MemoryManagerTest;
namespace Engine
{
    class Root;
}
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

        void *allocateStack(std::uint32_t size_bytes, size_t alignment = alignof(std::max_align_t));
        void freeStackMemory(void* ptr);
        
        void* allocateStackUnaligned(std::uint32_t size_bytes);
        void freeStackMemoryUnaligned(void* ptr);

        void freeStack();

    private:
        StackAllocater *stackAllocater_;

        explicit MemoryManager(MemoryManagerContext *ctx);
        ~MemoryManager();
    };
}