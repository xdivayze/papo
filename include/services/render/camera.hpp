#pragma once

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/quaternion_common.hpp"
#include "glm/gtc/quaternion.hpp"
#include "services/render/renderable_object.hpp"
#include "utils/exception.hpp"
#include <cstddef>
namespace Runtime {
// camera with no pitch limit
class ICamera : public AbstractMoveableObject {
public:
  static constexpr const char *TAG = "ICamera Class";

  // standard view matrix from quaternions
  virtual inline glm::mat4 viewMatrix() const {
    glm::mat4 rot = glm::mat4_cast(glm::conjugate(rotationQuat()));
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), -coordinates());
    return rot * trans;
  }

  virtual inline glm::mat4 projectionMatrix() const {
    return glm::perspective(fov_, (float)width_ / height_, nearPlane_,
                            farPlane_);
  }

  virtual inline void setFov(float fovDeg) {
    if (fovDeg >= 90.0f)
      throw Util::PapoException(TAG,
                                "setFov::FOV cannot be larger than 90 degrees");
    fov_ = fovDeg;
  }

  virtual inline void setWidth(std::size_t width) { width_ = width; }

  virtual inline void setHeight(std::size_t height) { height_ = height; }

  virtual inline void setNearPlane(float nearPlane) {
    if (nearPlane >= farPlane_)
      throw Util::PapoException(
          TAG, "setNearPlane::near plane cannot be further than far plane");
    nearPlane_ = nearPlane;
  }
  virtual inline void setFarPlane(float farPlane) {
    if (farPlane <= nearPlane_)
      throw Util::PapoException(
          TAG, "setFarPlane::far plane cannot be closer than near plane");
    farPlane_ = farPlane;
  }

  virtual inline float fov() const noexcept { return fov_; }

  virtual inline std::size_t width() const noexcept { return width_; }
  virtual inline std::size_t height() const noexcept { return height_; }

  virtual inline float nearPlane() const noexcept { return nearPlane_; }
  virtual inline float farPlane() const noexcept { return farPlane_; }

  explicit inline ICamera(std::size_t width, std::size_t height)
      : width_(width), height_(height) {}

  ~ICamera() = default;

private:
  float nearPlane_ = 0.1f;
  float farPlane_ = 100.0f;

  float fov_ = 45.0f;

  std::size_t width_;
  std::size_t height_;

  glm::vec3 cameraUp_ = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 cameraFront_ = glm::vec3(0.0f, 0.0f, -1.0f);
};

} // namespace Runtime