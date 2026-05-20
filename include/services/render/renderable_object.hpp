#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include "services/event/papo_event.hpp"
#include "services/render/asset_manager.hpp"
#include "services/render/model.hpp"

namespace Runtime {

class AbstractMoveableObject : public PapoEvent::IPapoEventListener {
public:
  // Subscribes to TimeManager::LastFrameTimeUpdatedEventID on the global
  // event bus. Each event pull-fetches the latest frame time from
  // Engine::Root::get().getTimeManager() and stores it as deltaTime().
  AbstractMoveableObject();

  // Note: IPapoEventListener lacks a virtual destructor, so this can't be
  // marked `override`. Still virtual so RenderableObject's override works.
  virtual ~AbstractMoveableObject();

  // The event bus stores a reference to `*this` while subscribed; copying or
  // moving would silently break that reference.
  AbstractMoveableObject(const AbstractMoveableObject &) = delete;
  AbstractMoveableObject &operator=(const AbstractMoveableObject &) = delete;
  AbstractMoveableObject(AbstractMoveableObject &&) = delete;
  AbstractMoveableObject &operator=(AbstractMoveableObject &&) = delete;

  virtual glm::vec3 coordinates() const;
  virtual glm::vec3 rotation() const;
  virtual glm::quat rotationQuat() const;
  virtual glm::vec3 scaling() const;
  virtual float deltaTime() const;

  // Manual setter. Useful for tests or callers that drive time themselves.
  // The bus subscription path overwrites this on the next
  // LastFrameTimeUpdated event.
  virtual void setDeltaTime(float dt);

  // IPapoEventListener — refreshes deltaTime_ from
  // Engine::Root::get().getTimeManager(). The event payload is ignored
  // (LastFrameTimeUpdated has no payload).
  void eventCall(void *payload) override;

  virtual void setCoordinates(glm::vec3 coordinates);
  virtual void setRotation(glm::vec3 rotationRad);
  virtual void setRotation(glm::quat rotationQuat);
  virtual void setScaling(glm::vec3 scaling);

  // Translate by `speed * deltaTime` (world frame).
  virtual void step(glm::vec3 speed);

  // Compose `delta` into the current rotation in the body's local frame
  // (post-multiply). `rotationEuler` is in radians.
  virtual void rotateLocal(glm::vec3 rotationEuler);

  // Compose `delta` into the current rotation in the world frame
  // (pre-multiply). `rotationEuler` is in radians.
  virtual void rotateWorld(glm::vec3 rotationEuler);

  // Rotate around a world-axis (through the origin) at `angularSpeed`
  // (rad/sec). Rotates the orientation by `angularSpeed * deltaTime`; the
  // object's position is unchanged.
  virtual void rotateStepWorld(glm::vec3 axis, float angularSpeed);

  // Rotate (instantaneously) by `rotationEuler` (radians) pivoting around the
  // world-space point `pivot`. Updates both coordinates (orbit) and
  // orientation (world-frame rotation).
  virtual void rotateAroundPivot(glm::vec3 pivot, glm::vec3 rotationEuler);

  // Rotate one step around `axis` at `angularSpeed` (rad/sec) pivoting around
  // `pivot`. Step size is `angularSpeed * deltaTime`. Updates both coordinates
  // (orbit) and orientation (world-frame rotation).
  virtual void rotateStepAroundPivot(glm::vec3 pivot, glm::vec3 axis,
                                     float angularSpeed);

private:
  // Default to the identity transform so a default-constructed
  // AbstractMoveableObject has a sensible initial state. RenderableObject's
  // ctors immediately overwrite these via the setters.
  glm::vec3 coordinates_{0.0f};

  glm::vec3 rotationRad_{0.0f};
  glm::quat rotationQuat_{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z = identity

  glm::vec3 scaling_{1.0f};

  float deltaTime_ = 0.0f;
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
