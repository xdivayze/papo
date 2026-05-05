#pragma once

#include "glad/gl.h"

#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "utils/exception.hpp"
#include <cstddef>

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

  // mesh object to be copied to the GPU memory bound by the passed vertex and
  // element objects
  Mesh(TVertex *vertices, unsigned int vertexCount, unsigned int *indices,
       unsigned int indexCount, unsigned int VAO, unsigned int VBO,
       unsigned int EBO)
      : vertices_(vertices), vertexCount_(vertexCount), indices_(indices),
        VAO_(VAO), VBO_(VBO), EBO_(EBO), indexCount_(indexCount) {}

  // mesh object needs to be created in the GPU memory
  Mesh(TVertex *vertices, unsigned int vertexCount, unsigned int *indices,
       unsigned int indexCount);

  // mesh object already in the GPU memory
  Mesh(unsigned int VAO, unsigned int VBO, unsigned int EBO,
       unsigned int indexCount)
      : VAO_(VAO), VBO_(VBO), EBO_(EBO), indexCount_(indexCount),
        preLoaded_(true) {}

  ~Mesh();

private:
  void Load();

  TVertex *vertices_;
  unsigned int vertexCount_;

  unsigned int *indices_;

  unsigned int VAO_, VBO_, EBO_;
  unsigned int indexCount_;

  bool preLoaded_ = false;
};
} // namespace Service

template <VertexTypes::VertexLayout TVertex>
void Service::Mesh<TVertex>::Load() {
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
                             unsigned int *indices, unsigned int indexCount) {
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

  glGenVertexArrays(1, &VAO_);
  glGenBuffers(1, &VBO_);
  glGenBuffers(1, &EBO_);
}

template <VertexTypes::VertexLayout TVertex> Service::Mesh<TVertex>::~Mesh() {
  // TODO
}
