#pragma once
#include <cstdlib>
#include <malloc.h>
#include <algorithm>
#include <mutex>
#include "../../utils/memory_utils.hpp"

class PoolAllocaterTest;

namespace Memory
{
    class PoolManager;

    class IPool
    {
    public:
        friend class PoolManager;
        virtual size_t capacity() const = 0;

        virtual ~IPool() = default;

    private:
        virtual void *alloc() = 0;
        virtual void free(void *ptr) = 0;
    };

    template <typename T>
    class PoolAllocater : public IPool
    {
    private:
        struct FreeNode
        {
            FreeNode *next;
        };

        static constexpr size_t chunkSize_ = Util::align_up(std::max(sizeof(T), sizeof(void *)), alignof(T));

        std::byte *memoryBlock_;
        FreeNode *freeHead_;
        size_t poolCapacity_;
        void *rawBlock_;

        // Serializes the intrusive free-list mutation so concurrent
        // acquire/release of THIS pool (e.g. parallel model loads) is safe.
        // Per-pool: distinct types never contend on each other.
        std::mutex allocMutex_;

        PoolAllocater(size_t capacity) : poolCapacity_(capacity)
        {
            rawBlock_ = malloc(capacity * chunkSize_ + alignof(T) - 1);
            memoryBlock_ = Util::AlignPointer<std::byte>(static_cast<std::byte *>(rawBlock_), alignof(T));

            freeHead_ = reinterpret_cast<FreeNode *>(memoryBlock_);
            FreeNode *curr = freeHead_;
            for (int i = 0; i < static_cast<int>(capacity); i++)
            {
                curr->next = reinterpret_cast<FreeNode *>(
                    memoryBlock_ + i * chunkSize_);
                curr = curr->next;
            }
            curr->next = nullptr;
        }

        ~PoolAllocater() override
        {
            ::free(rawBlock_);
        }

        void *alloc() override
        {
            std::lock_guard<std::mutex> lg(allocMutex_);
            if (freeHead_ == nullptr)
                return nullptr;
            FreeNode *chunk = freeHead_;
            freeHead_ = chunk->next;
            return static_cast<void *>(chunk);
        }

        void free(void *ptr) override
        {
            std::lock_guard<std::mutex> lg(allocMutex_);
            FreeNode *chunk = static_cast<FreeNode *>(ptr);
            chunk->next = freeHead_;
            freeHead_ = chunk;
        }

    public:
        friend class ::PoolAllocaterTest;
        friend class PoolManager;

        size_t capacity() const override
        {
            return poolCapacity_;
        }
    };
}
