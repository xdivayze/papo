#pragma once

#include "collections/robin_hood.hpp"
#include "model.hpp"
#include "services/memory/memory_manager.hpp"
#include "services/memory/pool_manager.hpp"
#include "services/memory/stack_allocater.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Service {

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

  ModelHandle<Model> modelFromFilePath(std::string_view filepath);

  AssetManager();
  ~AssetManager();

private:
  void *modelData_;
  std::size_t modelDataStackSize_;

  Memory::PoolManager &poolManager_;
  MemoryManager &memoryManager_;

  robin_hood::unordered_flat_map<std::string, ModelHandle<Model>> cache_;
};
} // namespace Service
