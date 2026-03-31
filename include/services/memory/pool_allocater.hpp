#pragma once
#include <cstdlib>
#include <memory>
#include <malloc.h>
#include "../../utils/memory_utils.hpp"
namespace Memory
{
    class IPool
    {
    public:
        virtual ~IPool() = default;
        virtual void *alloc() = 0;
        virtual void free(void *ptr) = 0;
        virtual size_t capacity() const = 0;
    };

    template <typename T>
    class PoolAllocater : public IPool
    {
    private:
        struct FreeNode
        {
            FreeNode *next;
        };

        static constexpr size_t chunkSize_ = align_up(max(sizeof(T), sizeof(void *)), alignof(T));

        std::byte *memoryBlock_;
        FreeNode *freeHead_;
        size_t poolCapacity_;
        void *rawBlock_;

    public:
        PoolAllocater(size_t capacity) : poolCapacity_(capacity)
        {
            rawBlock_ = malloc(capacity * chunkSize_ + alignof(T) - 1);
            memoryBlock_ = Util::AlignPointer<std::byte>(static_cast<std::byte *>(rawBlock_), alignof(T));

            freeHead_ = static_cast<FreeNode *>(memoryBlock_);
            FreeNode *curr = freeHead_;
            for (int i = 0; i < capacity; i++)
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
            if (freeHead_ == nullptr)
                return nullptr;
            FreeNode *chunk = freeHead_;
            freeHead_ = chunk->next;
            return static_cast<void *>(chunk);
        }

        void free(void *ptr) override
        {
            FreeNode *chunk = static_cast<FreeNode *>(ptr);
            chunk->next = freeHead_;
            freeHead_ = chunk;
        }

        size_t capacity() const override
        {
            return poolCapacity_;
        }
    };
}