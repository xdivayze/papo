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

  static constexpr const char *TAG = "MODEL CLASS";

  static constexpr std::size_t INLINE_MESH_CAP = 32;
  static constexpr std::size_t INLINE_MATERIAL_CAP = 16;
  static constexpr std::size_t INLINE_INSTANCE_CAP = 64;

  // load all meshes to the GPU. Release meshes to the pool if clean cpu data.
  std::uint32_t Load(bool cleanCPUData);

  // Stack holds only transient vertex/index buffers; Load(true) reclaims
  // the entire stack via stack_->freeToMarker(transientMarker_). The
  // pointer arrays + instance array live inline in Model (or on heap if
  // their counts exceed the inline caps), so the Model object survives
  // independently of the stack.
  Model(Memory::PoolManager &poolManager, StackAllocater *stack,
        std::string_view filepath, std::uint32_t id);

  ~Model();

  std::uint32_t transientMarker() const { return transientMarker_; }
  StackAllocater *stack() const { return stack_; }

  std::uint32_t id() const { return id_; }

private:
  template <typename T>
  static T *stackAlloc(StackAllocater *stack, std::size_t count);

  std::uint32_t id_;

  Memory::PoolManager &poolManager_;
  StackAllocater *stack_ = nullptr;
  std::uint32_t transientMarker_ = 0;

  // Inline storage for the common case. If counts exceed caps, the
  // corresponding active pointer points to a heap allocation instead.
  Mesh<VertexTypes::Vertex1P1N1UV> *meshesInline_[INLINE_MESH_CAP];
  Material *materialsInline_[INLINE_MATERIAL_CAP];
  MeshInstance instancesInline_[INLINE_INSTANCE_CAP];

  Mesh<VertexTypes::Vertex1P1N1UV> **meshes_ = nullptr;
  std::size_t meshCount_ = 0;

  Material **materials_ = nullptr;
  std::size_t materialCount_ = 0;

  MeshInstance *instances_ = nullptr;
  std::size_t instanceCount_ = 0;

  bool loaded_ = false;
};
} // namespace Service
