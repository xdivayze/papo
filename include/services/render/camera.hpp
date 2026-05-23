#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/quaternion_float.hpp"
#include "services/render/renderable_object.hpp"
namespace Runtime {
class Camera : public AbstractMoveableObject {
public:
  constexpr void setCameraPosition(glm::vec3 cameraPos) {
    cameraPosition_ = cameraPos;
  }

  constexpr glm::quat getRotationQuat() const { return rotationQuat_; }
  constexpr glm::mat4 getViewMatrix() const { return viewMatrix_; }

  glm::vec3 getRotationEuler() const;

  void setRotation(glm::vec3 rotationEuler);
  void setRotation(glm::quat rotationQuat);

  void rotate(glm::vec3 changeEuler);
  void rotate(glm::quat changeRotation);

  void moveCamera(glm::vec3 speed); // move one step

private:
  glm::quat rotationQuat_;

  glm::vec3 cameraUp_ = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 cameraFront_ = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 cameraPosition_ = glm::vec3(0.0f, 0.0f, 0.0f);

  glm::mat4 viewMatrix_;
};
} // namespace Runtime