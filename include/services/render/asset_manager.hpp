#pragma once

#include "mesh.hpp"

namespace Service {

class AssetManager {
public:
  template <typename TVertex> void LoadMeshToGPU(Mesh<TVertex> mesh);
  template <typename TVertex> Mesh<TVertex> CreateMeshFromFileHandle();

private:
};
} // namespace Service
