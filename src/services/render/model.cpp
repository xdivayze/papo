#include "services/render/model.hpp"
#include "assimp/Importer.hpp"
#include "assimp/material.h"
#include "assimp/mesh.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "services/memory/stack_allocater.hpp"
#include "utils/exception.hpp"
#include <algorithm>
#include <new>

namespace {

using MeshT = Service::Mesh<VertexTypes::Vertex1P1N1UV>;
using VertexT = VertexTypes::Vertex1P1N1UV;

std::size_t countInstances(const aiNode *node) {
  std::size_t count = node->mNumMeshes;
  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    count += countInstances(node->mChildren[i]);
  }
  return count;
}

// assimp matrices are row-major; glm is column-major
glm::mat4 toGlm(const aiMatrix4x4 &m) {
  return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3,
                   m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
}

void buildInstances(const aiNode *node, const aiMatrix4x4 &parentTransform,
                    const aiScene *scene, Service::MeshInstance *instances,
                    std::size_t &cursor) {
  aiMatrix4x4 transform = parentTransform * node->mTransformation;
  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    const aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
    Service::MeshInstance &inst = instances[cursor++];
    inst.meshIndex = node->mMeshes[i];
    inst.materialIndex = mesh->mMaterialIndex;
    inst.transform = toGlm(transform);
  }
  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    buildInstances(node->mChildren[i], transform, scene, instances, cursor);
  }
}

} // namespace

namespace Service {

template <typename T>
T *Model::stackAlloc(StackAllocater *stack, std::size_t count) {
  if (count == 0)
    return nullptr;
  return static_cast<T *>(stack->allocAligned(
      static_cast<std::uint32_t>(sizeof(T) * count), alignof(T)));
}

Model::Model(Memory::PoolManager &poolManager, StackAllocater *stack,
             std::string_view filepath)
    : poolManager_(poolManager), stack_(stack) {
  Assimp::Importer import;
  const aiScene *scene = import.ReadFile(
      filepath.data(), aiProcess_Triangulate | aiProcess_FlipUVs);
  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
      !scene->mRootNode) {
    throw Util::PapoException(
        TAG, "model couldn't be loaded, scene initialization failed");
  }

  meshCount_ = scene->mNumMeshes;
  materialCount_ = scene->mNumMaterials;
  instanceCount_ = countInstances(scene->mRootNode);

  // Persistent block: pointer arrays + inline instance array. These live
  // for the lifetime of the model.
  meshes_ = stackAlloc<MeshT *>(stack_, meshCount_);
  materials_ = stackAlloc<Material *>(stack_, materialCount_);
  instances_ = stackAlloc<MeshInstance>(stack_, instanceCount_);

  // Everything allocated after this marker is transient: per-mesh vertex
  // and index buffers needed only until GPU upload. Load(cleanCPUData=true)
  // can stack_->freeToMarker(transientMarker_) to reclaim them.
  transientMarker_ = stack_->getMarker();

  for (std::size_t i = 0; i < meshCount_; i++) {
    const aiMesh *aim = scene->mMeshes[i];
    unsigned int vertexCount = aim->mNumVertices;
    unsigned int indexCount = 0;
    for (unsigned int f = 0; f < aim->mNumFaces; f++) {
      indexCount += aim->mFaces[f].mNumIndices;
    }

    VertexT *verts = stackAlloc<VertexT>(stack_, vertexCount);
    unsigned int *inds = stackAlloc<unsigned int>(stack_, indexCount);

    const bool hasNormals = aim->HasNormals();
    const aiVector3D *uv0 = aim->mTextureCoords[0];
    const aiVector3D *uv1 = aim->mTextureCoords[1];
    for (unsigned int v = 0; v < vertexCount; v++) {
      verts[v].position = {aim->mVertices[v].x, aim->mVertices[v].y,
                           aim->mVertices[v].z};
      verts[v].normal =
          hasNormals
              ? glm::vec3{aim->mNormals[v].x, aim->mNormals[v].y,
                          aim->mNormals[v].z}
              : glm::vec3{0.0f};
      verts[v].diffuseMapCoords =
          uv0 ? glm::vec2{uv0[v].x, uv0[v].y} : glm::vec2{0.0f};
      verts[v].specularMapCoords =
          uv1 ? glm::vec2{uv1[v].x, uv1[v].y} : verts[v].diffuseMapCoords;
    }

    unsigned int idxCursor = 0;
    for (unsigned int f = 0; f < aim->mNumFaces; f++) {
      const aiFace &face = aim->mFaces[f];
      for (unsigned int j = 0; j < face.mNumIndices; j++) {
        inds[idxCursor++] = face.mIndices[j];
      }
    }

    MeshT *slot = poolManager_.acquireFromPool<MeshT>();
    if (!slot)
      throw Util::PapoException(TAG, "Mesh pool exhausted while loading model");
    meshes_[i] = new (slot) MeshT(verts, vertexCount, inds, indexCount);
  }

  for (std::size_t i = 0; i < materialCount_; i++) {
    const aiMaterial *aim = scene->mMaterials[i];
    Material *slot = poolManager_.acquireFromPool<Material>();
    if (!slot)
      throw Util::PapoException(TAG,
                                "Material pool exhausted while loading model");
    Material *mat = new (slot) Material{};

    aiColor3D color;
    if (aim->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
      mat->ambientColor = {color.r, color.g, color.b};
    }
    if (aim->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
      mat->diffuseColor = {color.r, color.g, color.b};
    }
    if (aim->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
      mat->specularColor = {color.r, color.g, color.b};
    }
    float shininess;
    if (aim->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
      mat->shininess = shininess;
    }
    materials_[i] = mat;
  }

  std::size_t instanceCursor = 0;
  aiMatrix4x4 identity;
  buildInstances(scene->mRootNode, identity, scene, instances_, instanceCursor);

  // Sort instances by (materialIndex, meshIndex) so the render loop sees
  // material binds coalesced and mesh-pointer chases run in order.
  std::sort(instances_, instances_ + instanceCount_,
            [](const MeshInstance &a, const MeshInstance &b) {
              if (a.materialIndex != b.materialIndex)
                return a.materialIndex < b.materialIndex;
              return a.meshIndex < b.meshIndex;
            });

  loaded_ = false;
}

Model::~Model() {
  for (std::size_t i = 0; i < meshCount_; i++) {
    meshes_[i]->~MeshT();
    poolManager_.releaseToPool(meshes_[i]);
  }
  for (std::size_t i = 0; i < materialCount_; i++) {
    materials_[i]->~Material();
    poolManager_.releaseToPool(materials_[i]);
  }
  // MeshInstance is trivially destructible. The stack itself is owned by
  // the caller (AssetManager) — it clears/reclaims after destruction.
}

} // namespace Service
