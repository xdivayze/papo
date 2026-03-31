#pragma once
#include "pool_allocater.hpp"
#include <unordered_map>
#include <cstddef>
#include <typeindex>
#include <memory>
#include <cassert>
#include "../../utils/exception.hpp"
#include "../../collections/robin_hood.hpp"
constexpr const char *TAG = "Pool Manager";

namespace Memory
{
    class PoolManager
    {

    public:
        // check if a pool belonging to type T exists in the manager's map
        template <typename T>
        bool hasPool() const
        {
            auto key = std::type_index(typeid(T));
            return pools_.contains(key);
        }

        // acquire a single chunk of type T from pool
        template <typename T>
        T *acquireFromPool()
        {
            IPool &foundPool = getPoolAllocater<T>();
            return static_cast<T *>(foundPool.alloc());
        }

        // release a single chunk of type T back into the pool
        template <typename T>
        void releaseToPool(T *ptr)
        {
            IPool &foundPool = getPoolAllocater<T>();
            foundPool.free(ptr);
        }

        // register a new pool of capacity capacity to the manager's map
        template <typename T>
        void registerPool(std::size_t capacity)
        {
            auto key = std::type_index(typeid(T));
            if (pools_.contains(key))
                throw Util::LogicException(TAG, "Pool already exists in pool manager");

            pools_[key] = std::unique_ptr<IPool>(new PoolAllocater<T>(capacity));
        }

        /*
            registerIPool transfers the ownership of pool to pools_[key].
            By doing so, it registers the pool for the type with the type index key.

            The caller must ensure that the passed class implementing IPool properly manages the memory
            as this IPool object will be the default pool allocater for the type
        */
        template <typename T>
        void registerIPool(std::unique_ptr<IPool> pool)
        {
            assert(pool != nullptr);
            auto key = std::type_index(typeid(T));
            if (pools_.contains(key))
                throw Util::LogicException(TAG, "Pool already exists in pool manager");

            pools_[key] = std::move(pool);
        }

        // remove the pool belonging to type T from the manager's map and free its memory
        template <typename T>
        void removePoolAllocater()
        {
            auto key = std::type_index(typeid(T));
            auto foundPool = pools_.find(key);
            if (foundPool == pools_.end())
                throw Util::LogicException(TAG, "The pool that is to be removed doesn't exist in the manager");

            pools_.erase(foundPool);
        }

    private:
        template <typename T>
        IPool &getPoolAllocater()
        {
            auto key = std::type_index(typeid(T));
            auto foundPool = pools_.find(key);
            if (foundPool == pools_.end())
                throw Util::LogicException(TAG, "The pool that is to be retrieved doesn't exist in the manager");

            return *foundPool->second;
        }

        robin_hood::unordered_flat_map<std::type_index, std::unique_ptr<IPool>> pools_; 
    };

}