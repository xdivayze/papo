#pragma once
#include "../../collections/robin_hood.hpp"
#include "../../utils/exception.hpp"
#include "pool_allocater.hpp"
#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>

namespace Memory {
class PoolManager {

public:
  static constexpr const char *TAG = "Pool Manager";
  // check if a pool belonging to type T exists in the manager's map
  template <typename T> bool hasPool() const {
    auto key = std::type_index(typeid(T));
    std::shared_lock<std::shared_mutex> lk(poolsMutex_);
    return pools_.contains(key);
  }

  // acquire a single chunk of type T from pool
  template <typename T> T *acquireFromPool() {
    // Shared lock held across alloc(): a concurrent removePoolAllocater
    // (exclusive lock) cannot erase the pool mid-use, so the IPool& stays
    // valid. The per-pool mutex inside PoolAllocater makes the concurrent
    // free-list mutation safe between shared-lock holders.
    std::shared_lock<std::shared_mutex> lk(poolsMutex_);
    IPool &foundPool = getPoolAllocater<T>();
    return static_cast<T *>(foundPool.alloc());
  }

  // release a single chunk of type T back into the pool
  template <typename T> void releaseToPool(T *ptr) {
    std::shared_lock<std::shared_mutex> lk(poolsMutex_);
    IPool &foundPool = getPoolAllocater<T>();
    foundPool.free(ptr);
  }

  // register a new pool of capacity capacity to the manager's map
  template <typename T> void registerPool(std::size_t capacity) {
    auto key = std::type_index(typeid(T));
    std::unique_lock<std::shared_mutex> lk(poolsMutex_);
    if (pools_.contains(key))
      throw Util::LogicException(TAG, "Pool already exists in pool manager");

    pools_[key] = std::unique_ptr<IPool>(new PoolAllocater<T>(capacity));
  }

  /*
      registerIPool transfers the ownership of pool to pools_[key].
      By doing so, it registers the pool for the type with the type index key.

      The caller must ensure that the passed class implementing IPool properly
     manages the memory as this IPool object will be the default pool allocater
     for the type.

      NOTE: only the built-in PoolAllocater synchronizes its alloc()/free().
     A custom IPool registered here must provide its own internal
     synchronization if it will be used from multiple threads.
  */
  template <typename T> void registerIPool(std::unique_ptr<IPool> pool) {
    assert(pool != nullptr);
    auto key = std::type_index(typeid(T));
    std::unique_lock<std::shared_mutex> lk(poolsMutex_);
    if (pools_.contains(key))
      throw Util::LogicException(TAG, "Pool already exists in pool manager");

    pools_[key] = std::move(pool);
  }

  // remove the pool belonging to type T from the manager's map and free its
  // memory
  template <typename T> void removePoolAllocater() {
    auto key = std::type_index(typeid(T));
    std::unique_lock<std::shared_mutex> lk(poolsMutex_);
    auto foundPool = pools_.find(key);
    if (foundPool == pools_.end())
      throw Util::LogicException(
          TAG, "The pool that is to be removed doesn't exist in the manager");

    pools_.erase(foundPool);
  }

private:
  // No-lock lookup helper: the caller MUST already hold poolsMutex_ (shared
  // is sufficient — the map is only structurally modified under the
  // exclusive lock by register*/removePoolAllocater).
  template <typename T> IPool &getPoolAllocater() {
    auto key = std::type_index(typeid(T));
    auto foundPool = pools_.find(key);
    if (foundPool == pools_.end())
      throw Util::LogicException(
          TAG, "The pool that is to be retrieved doesn't exist in the manager");

    return *foundPool->second;
  }

  mutable std::shared_mutex poolsMutex_;

  robin_hood::unordered_flat_map<std::type_index, std::unique_ptr<IPool>>
      pools_;
};

} // namespace Memory