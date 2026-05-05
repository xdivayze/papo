#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "services/memory/pool_manager.hpp"
#include "services/render/mesh.hpp"
#include <cstddef>
#include <cstdint>

namespace Service {

struct Material; 

struct MeshInstance {
  uint32_t meshIndex;
  uint32_t materialIndex;
  glm::mat4 transform;
};

class AssetManager;
class Model {
public:
  friend class AssetManager;

  // load all meshes to the GPU. Release meshes to the pool if clean cpu data.
  void Load(bool cleanCPUData);

  Model(Memory::PoolManager &poolManager,
        Mesh<VertexTypes::Vertex1P1N1UV> *meshes, std::size_t meshCount,
        Material *materials, std::size_t materialCount,
        MeshInstance *instances, std::size_t instanceCount,
        bool loaded = false); // fully instantiated model

  ~Model();

private:
  Memory::PoolManager &poolManager_;

  Mesh<VertexTypes::Vertex1P1N1UV> *meshes_;
  std::size_t meshCount_;

  Material *materials_;
  std::size_t materialCount_;

  MeshInstance *instances_;
  std::size_t instanceCount_;

  bool loaded_;
};
} // namespace Service
