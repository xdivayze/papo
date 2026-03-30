#pragma once
#include <cstdlib>
#include <memory>
#include <malloc.h>
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
        FreeNode *freeHead;
        size_t poolCapacity;

    public:
        PoolAllocater(size_t capacity) : poolCapacity(capacity)
        {
            memoryBlock_ = static_cast<std::byte *>(malloc(capacity * chunkSize_));
            freeHead = static_cast<FreeNode *>(memoryBlock_);
            FreeNode *curr = freeHead;
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
            delete memoryBlock_;
        }

        void *alloc() override
        {
            if (freeHead == nullptr)
                return nullptr;
            FreeNode *chunk = freeHead;
            freeHead = chunk->next;
            return static_cast<void *>(chunk);
        }

        void free(void *ptr) override
        {
            FreeNode *chunk = static_cast<FreeNode *>(ptr);
            chunk->next = freeHead;
            freeHead = chunk;
        }

        size_t capacity() const override
        {
            return poolCapacity;
        }
    };
}