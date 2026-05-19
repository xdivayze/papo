#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/gtc/quaternion.hpp"
#include "services/render/asset_manager.hpp"
#include "services/render/model.hpp"
namespace Service {
class RenderableObject {
public:
  virtual glm::vec3 coordinates() const { return coordinates_; }

  virtual glm::vec3 rotation() const { return rotationRad_; }
  virtual glm::quat rotationQuat() const { return rotationQuat_; }

  virtual glm::vec3 scaling() const { return scaling_; }

  virtual glm::mat4 getTransform() const { return transform_; }

  virtual void setCoordinates(glm::vec3 coordinates) {
    coordinates_ = coordinates;
    updateTransform();
  }
  virtual void setRotation(glm::vec3 rotationRad) {
    rotationRad_ = rotationRad;
    rotationQuat_ = glm::quat(rotationRad);
    updateTransform();
  }

  virtual void setRotation(glm::quat rotationQuat) {
    rotationRad_ = glm::eulerAngles(rotationQuat);
    rotationQuat_ = rotationQuat;
    updateTransform();
  }

  virtual void setScaling(glm::vec3 scaling) {
    scaling_ = scaling;
    updateTransform();
  }

  RenderableObject(AssetManager::ModelHandle<Model> modelHandle, glm::vec3 coordinates,
                   glm::vec3 rotationRad, glm::vec3 scaling);
  RenderableObject(AssetManager::ModelHandle<Model> modelHandle, glm::vec3 coordinates,
                   glm::quat rotationQuat, glm::vec3 scaling);
  RenderableObject(AssetManager::ModelHandle<Model> modelHandle, glm::vec3 coordinates,
                   glm::vec3 rotationRad, glm::quat rotationQuat,
                   glm::vec3 scaling);

  virtual ~RenderableObject() = default;

private:
  void updateTransform() {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), coordinates_);
    glm::mat4 R = glm::mat4_cast(rotationQuat_);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scaling_);
    transform_ = T * R * S;
  }

  AssetManager::ModelHandle<Model> modelHandle_;
  glm::vec3 coordinates_;

  glm::vec3 rotationRad_;
  glm::quat rotationQuat_;

  glm::vec3 scaling_;

  glm::mat4 transform_;
};
} // namespace Service