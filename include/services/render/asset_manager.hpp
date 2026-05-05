#pragma once

#include "collections/robin_hood.hpp"
#include "mesh.hpp"
#include "services/memory/memory_manager.hpp"
#include "services/memory/pool_manager.hpp"
#include "services/memory/stack_allocater.hpp"
#include "utils/exception.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Service {

class AssetManager {
public:
  static constexpr const char *TAG = "Asset Manager";

  template <typename T> struct MeshHandle {
    uint32_t id;
    T *ptr; // pointer to the pool allocated chunk that holds the mesh object

    bool isValid() const { return id < UINT32_MAX && ptr != nullptr; }
    T *operator->() { return ptr; }
    T &operator*() { return *ptr; }
  };

  template <VertexTypes::VertexLayout TVertex>
  void loadMeshToGPU(MeshHandle<Mesh<TVertex>> mesh, bool cleanLastCPUData);

  template <VertexTypes::VertexLayout TVertex>
  MeshHandle<Mesh<TVertex>> meshFromFilePath(std::string_view filepath);

  AssetManager();
  ~AssetManager();

private:
  void *meshData_;
  std::size_t meshDataStackSize_;

  Memory::PoolManager &poolManager_;
  MemoryManager &memoryManager_;

  robin_hood::unordered_flat_map<std::string,
                                 MeshHandle<VertexTypes::Vertex1P1N1UV>>
      cache_;
};
} // namespace Service

template <VertexTypes::VertexLayout TVertex>
Service::AssetManager::MeshHandle<Service::Mesh<TVertex>>
meshFromFilePath(std::string_view filepath) {
    
}

template <VertexTypes::VertexLayout TVertex>
void Service::AssetManager::loadMeshToGPU(MeshHandle<Mesh<TVertex>> mesh,
                                          bool cleanLastCPUData) {
  if (!mesh.isValid())
    throw Util::PapoException(TAG,
                              "mesh that is trying to be loaded is not valid");

  mesh->Load();
  if (cleanLastCPUData) {
    size_t ptr_last = mesh->indexCount_ * sizeof(unsigned int) +
                      mesh->vertexCount_ * sizeof(TVertex);
    memoryManager_.freeStackMemory(mesh.ptr - ptr_last);
    poolManager_.releaseToPool(mesh.ptr);
    mesh.ptr = nullptr;
  }
}