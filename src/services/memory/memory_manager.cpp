#include "services/memory/memory_manager.hpp"

constexpr const std::uint32_t DEFAULT_STACK_LOCK_SIZE_BYTES = (1 << 10) << 10; // 1M

namespace Services
{
    
    void *MemoryManager::allocateStack(std::uint32_t size_bytes)
    {
        return stackAllocater_->alloc(size_bytes);
    }

    void MemoryManager::freeStack()
    {
        return stackAllocater_->clear();
    }

    void MemoryManager::freeStackToMarker(StackAllocater::Marker marker)
    {
        return stackAllocater_->freeToMarker(marker);
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
