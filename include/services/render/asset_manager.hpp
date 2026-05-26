#pragma once

#include "collections/robin_hood.hpp"
#include "model.hpp"
#include "services/memory/memory_manager.hpp"
#include "services/memory/pool_manager.hpp"
#include "services/memory/stack_allocater.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace async {
class ThreadPoolManager;
}

namespace Service {

// TODO switch to shared pointers for model lifetime

/*
! MODELS ARE NOT DESTROYED UNLESS EXPLICITLY DONE BY THE SCENE WHICH WOULD
! CAUSE CONFLICTS WITH THE CACHE SYSTEM
*/

//TODO split the asset manager to its run time and compile time (for the game ) components
// 7.2 in the book

//TODO add VFS
class AssetManager {
public:
  static constexpr const char *TAG = "Asset Manager";

  template <typename T> struct ModelHandle {
    uint32_t id;
    T *ptr; // pointer to the pool allocated chunk that holds the model object

    bool isValid() const { return id < UINT32_MAX && ptr != nullptr; }
    T *operator->() { return ptr; }
    T &operator*() { return *ptr; }
  };

  void loadModelToGPU(ModelHandle<Model> model, bool cleanLastCPUData);

  // When deferGL is true the Model is built without generating GL buffers
  // (safe off the render thread). The buffers are created lazily on the
  // first loadModelToGPU, or explicitly via Model::initializeMeshBuffers().
  ModelHandle<Model> modelFromFilePath(std::string_view filepath,
                                       bool deferGL = false);

  // Same as modelFromFilePath but the load runs on a thread-pool worker.
  // The returned future yields the handle (or rethrows the load exception
  // via .get()). Thread-safe against concurrent sync/async calls.
  std::future<ModelHandle<Model>>
  modelFromFilePathAsync(async::ThreadPoolManager &pool,
                         std::string_view filepath);

  AssetManager(Memory::PoolManager &poolManager, MemoryManager &memoryManager_,
               size_t nstacks);
  ~AssetManager();

private:
  void *modelData_;
  std::size_t modelDataStackSize_;

  std::uint32_t idCounter_ = 0;

  Memory::PoolManager &poolManager_;
  MemoryManager &memoryManager_;

  robin_hood::unordered_flat_map<std::uint32_t, StackAllocater *>
      allocatorLeaseMap; // stack allocator lease map
  robin_hood::unordered_flat_map<std::string, ModelHandle<Model>>
      cache_; // asset cache

  std::vector<StackAllocater *> freeStacks_; // available stack leases
  std::mutex leaseMutex_;
  std::condition_variable leaseCv_;
};
} // namespace Service
