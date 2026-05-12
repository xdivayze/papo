#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "services/memory/pool_manager.hpp"
#include "services/render/mesh.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Service {

class StackAllocater;

struct Material {
  glm::vec3 ambientColor = glm::vec3(0.1f);
  glm::vec3 diffuseColor = glm::vec3(1.0f);
  glm::vec3 specularColor = glm::vec3(1.0f);
  float shininess = 32.0f;
};

struct MeshInstance {
  uint32_t meshIndex;
  uint32_t materialIndex;
  glm::mat4 transform;
};

class AssetManager;
class Model {
public:
  friend class AssetManager;

  static constexpr const char* TAG = "ASSET MANAGER";

  // load all meshes to the GPU. Release meshes to the pool if clean cpu data.
  void Load(bool cleanCPUData);

  Model(Memory::PoolManager &poolManager,
        Mesh<VertexTypes::Vertex1P1N1UV> **meshes, std::size_t meshCount,
        Material **materials, std::size_t materialCount,
        MeshInstance *instances, std::size_t instanceCount,
        bool loaded = false); // fully instantiated model

  // Caller (AssetManager) is responsible for handing in a stack with enough
  // capacity and for reclaiming/clearing it after the Model is destroyed.
  // The Model only allocates from it — it never frees on the stack.
  Model(Memory::PoolManager &poolManager, StackAllocater *stack,
        std::string_view filepath);

  ~Model();

  // Marker to where vertex/index data starts on the stack. Load(true) can
  // freeToMarker(this) on the stack to discard CPU-side mesh data after
  // upload, while keeping the pointer arrays + instances live.
  std::uint32_t transientMarker() const { return transientMarker_; }
  StackAllocater *stack() const { return stack_; }

private:
  template <typename T>
  static T *stackAlloc(StackAllocater *stack, std::size_t count);

  Memory::PoolManager &poolManager_;
  StackAllocater *stack_ = nullptr;
  std::uint32_t transientMarker_ = 0;

  Mesh<VertexTypes::Vertex1P1N1UV> **meshes_;
  std::size_t meshCount_;

  Material **materials_;
  std::size_t materialCount_;

  MeshInstance *instances_;
  std::size_t instanceCount_;

  bool loaded_;
};
} // namespace Service
