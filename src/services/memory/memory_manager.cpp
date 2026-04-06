#include "services/memory/memory_manager.hpp"

constexpr const std::uint32_t DEFAULT_STACK_LOCK_SIZE_BYTES = (1 << 10) << 10; // 1M

namespace Service
{

    void *MemoryManager::allocateStack(std::uint32_t size_bytes, size_t alignment)
    {
        return stackAllocater_->allocAligned(size_bytes, alignment);
    }

    void MemoryManager::freeStack()
    {
        return stackAllocater_->clear();
    }

    void MemoryManager::freeStackMemory(void *ptr)
    {
        return stackAllocater_->freeAligned(ptr);
    }

    void *MemoryManager::allocateStackUnaligned(std::uint32_t size_bytes)
    {
        return stackAllocater_->alloc(size_bytes);
    }

    void MemoryManager::freeStackMemoryUnaligned(void *ptr)
    {
        return stackAllocater_->freeUnaligned(ptr);
    }

    MemoryManager::MemoryManager(MemoryManagerContext *ctx)
    {
        if (ctx == nullptr)

            stackAllocater_ = new StackAllocater(DEFAULT_STACK_LOCK_SIZE_BYTES);
        else
            stackAllocater_ = new StackAllocater(ctx->StackSize);
    }

    MemoryManager::~MemoryManager()
    {
        delete stackAllocater_;
    }
}
