#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include "services/render/asset_manager.hpp"
#include "services/render/model.hpp"

namespace Runtime {

class AbstractMoveableObject {
public:
  virtual ~AbstractMoveableObject() = default;

  virtual glm::vec3 coordinates() const;
  virtual glm::vec3 rotation() const;
  virtual glm::quat rotationQuat() const;
  virtual glm::vec3 scaling() const;

  // TODO add tests for this
  virtual void rotateLocal(glm::vec3 rotationEuler);
  // TODO add rotate one step in a given axis and angular speed
  // TODO add rotation around world axes

  virtual void setCoordinates(glm::vec3 coordinates);
  virtual void setRotation(glm::vec3 rotationRad);
  virtual void setRotation(glm::quat rotationQuat);
  virtual void setScaling(glm::vec3 scaling);

  // take one step
  virtual void step(glm::vec3 speed); // TODO implement and test

private:
  glm::vec3 coordinates_;

  glm::vec3 rotationRad_;
  glm::quat rotationQuat_;

  glm::vec3 scaling_;
};

class RenderableObject : public AbstractMoveableObject {
public:
  RenderableObject(
      Service::AssetManager::ModelHandle<Service::Model> modelHandle,
      glm::vec3 coordinates, glm::vec3 rotationRad, glm::vec3 scaling);
  RenderableObject(
      Service::AssetManager::ModelHandle<Service::Model> modelHandle,
      glm::vec3 coordinates, glm::quat rotationQuat, glm::vec3 scaling);
  RenderableObject(
      Service::AssetManager::ModelHandle<Service::Model> modelHandle,
      glm::vec3 coordinates, glm::vec3 rotationRad, glm::quat rotationQuat,
      glm::vec3 scaling);

  // Defaults to identity transform (origin, no rotation, unit scale).
  explicit RenderableObject(
      Service::AssetManager::ModelHandle<Service::Model> modelHandle);

  ~RenderableObject() override = default;

  virtual glm::mat4 getTransform() const;

  void setCoordinates(glm::vec3 coordinates) override;
  void setRotation(glm::vec3 rotationRad) override;
  void setRotation(glm::quat rotationQuat) override;
  void setScaling(glm::vec3 scaling) override;

private:
  void updateTransform();

  Service::AssetManager::ModelHandle<Service::Model> modelHandle_;

  glm::mat4 transform_;
};

} // namespace Runtime
