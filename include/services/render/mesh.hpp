#pragma once

#include "glad/gl.h"

#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "utils/exception.hpp"
#include <cstddef>

// TODO current setup has too much overhead on account of GL loading the data to
// the GPU through glBufferData this should be fixed bc even though i am getting
// claude to write the code just the pure effort it takes to architecture the
// memory hierarchy is exhausting. I am hoping to use custom memory layout in GL
// as well (one huge VBO EBO etc later allocated through allocaters)

class MeshTest;

namespace VertexTypes {

template <typename T>
concept VertexLayout = requires { T::SetupAttribs(); };

struct Vertex1P1N1UV {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 diffuseMapCoords;
  glm::vec2 specularMapCoords;

  static void SetupAttribs() {
    size_t stride = sizeof(Vertex1P1N1UV);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N1UV, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N1UV, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N1UV, diffuseMapCoords)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3, 2, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N1UV, specularMapCoords)));
  }
};

struct Vertex1P1N {
  glm::vec3 position;
  glm::vec3 normal;

  static void SetupAttribs() {
    size_t stride = sizeof(Vertex1P1N);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N, normal)));
  }
};

struct Vertex1P {
  glm::vec3 position;

  static void SetupAttribs() {
    size_t stride = sizeof(Vertex1P);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P, position)));
  }
};

typedef glm::vec4 ColorRGBA;

struct Vertex1P1N1C {
  glm::vec3 position;
  glm::vec3 normal;
  ColorRGBA color;

  static void SetupAttribs() {
    size_t stride = sizeof(Vertex1P1N1C);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N1C, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N1C, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 4, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void *>(offsetof(Vertex1P1N1C, color)));
  }
};
} // namespace VertexTypes

namespace Service {

template <VertexTypes::VertexLayout TVertex> class Mesh {
public:
  static constexpr const char *TAG = "Mesh Manager";

  friend class AssetManager;
  friend class Model;
  friend class ::MeshTest;

  // mesh object to be copied to the GPU memory bound by the passed vertex and
  // element objects
  Mesh(TVertex *vertices, unsigned int vertexCount, unsigned int *indices,
       unsigned int indexCount, unsigned int VAO, unsigned int VBO,
       unsigned int EBO)
      : vertices_(vertices), vertexCount_(vertexCount), indices_(indices),
        VAO_(VAO), VBO_(VBO), EBO_(EBO), indexCount_(indexCount),
        buffersInitialized_(true) {}

  // mesh object needs to be created in the GPU memory. When deferBufferInit
  // is true the VAO/VBO/EBO are NOT generated here (no GL call) — call
  // initBuffers() later on a thread with a current GL context. This lets the
  // CPU-side load run on a worker thread (deferred-GL design).
  Mesh(TVertex *vertices, unsigned int vertexCount, unsigned int *indices,
       unsigned int indexCount, bool deferBufferInit = false);

  // mesh object already in the GPU memory
  Mesh(unsigned int VAO, unsigned int VBO, unsigned int EBO,
       unsigned int indexCount)
      : VAO_(VAO), VBO_(VBO), EBO_(EBO), indexCount_(indexCount),
        preLoaded_(true), buffersInitialized_(true) {}

  ~Mesh();

private:
  void Load();

  // Generate the VAO/VBO/EBO. Idempotent: a no-op once buffersInitialized_.
  // Requires a current GL context on the calling thread.
  void initBuffers();

  TVertex *vertices_;
  unsigned int vertexCount_;

  unsigned int *indices_;

  unsigned int VAO_ = 0, VBO_ = 0, EBO_ = 0;
  unsigned int indexCount_;

  bool preLoaded_ = false;
  bool buffersInitialized_ = false;
};
} // namespace Service

template <VertexTypes::VertexLayout TVertex>
void Service::Mesh<TVertex>::initBuffers() {
  if (buffersInitialized_)
    return;
  glGenVertexArrays(1, &VAO_);
  glGenBuffers(1, &VBO_);
  glGenBuffers(1, &EBO_);
  buffersInitialized_ = true;
}

template <VertexTypes::VertexLayout TVertex>
void Service::Mesh<TVertex>::Load() {
  // Reflect deferred-GL: if buffers were never generated (built on a worker
  // thread), do it now — Load() always runs on the GL thread.
  if (!buffersInitialized_)
    initBuffers();

  glBindVertexArray(VAO_);
  glBindBuffer(GL_ARRAY_BUFFER, VBO_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);

  if (!preLoaded_) {
    glBufferData(GL_ARRAY_BUFFER, vertexCount_ * sizeof(vertices_[0]),
                 vertices_, GL_STATIC_DRAW);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount_ * sizeof(indices_[0]),
                 indices_, GL_STATIC_DRAW);
  }

  TVertex::SetupAttribs();

  glBindVertexArray(0);
}

template <VertexTypes::VertexLayout TVertex>
Service::Mesh<TVertex>::Mesh(TVertex *vertices, unsigned int vertexCount,
                             unsigned int *indices, unsigned int indexCount,
                             bool deferBufferInit) {
  if (vertexCount == 0) {
    throw Util::PapoException(TAG, "0 length vertex array not allowed");
  }
  if (indexCount == 0) {
    throw Util::PapoException(TAG, "0 length index array not allowed");
  }
  indices_ = indices;
  indexCount_ = indexCount;

  vertices_ = vertices;
  vertexCount_ = vertexCount;

  if (!deferBufferInit)
    initBuffers(); // immediate (default): generate GL objects now
}

template <VertexTypes::VertexLayout TVertex> Service::Mesh<TVertex>::~Mesh() {
  // Release the GL objects this mesh owns. glDelete* are no-ops on name 0
  // and silently ignore already-deleted names, so this is safe even if the
  // mesh was never uploaded. vertices_/indices_ are NOT freed here: they
  // point into the model's transient StackAllocater scratch, which is owned
  // and reclaimed by Model/StackAllocater, not by Mesh.
  glDeleteVertexArrays(1, &VAO_);
  glDeleteBuffers(1, &VBO_);
  glDeleteBuffers(1, &EBO_);
}
