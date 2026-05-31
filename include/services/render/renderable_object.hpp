#pragma once

#include <cstdint>
#include <memory>

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include "services/event/papo_event.hpp"
#include "services/render/asset_manager.hpp"
#include "services/render/model.hpp"

namespace Runtime {

// Caches the latest frame delta from the global event bus.
// AbstractMoveableObject owns one of these via unique_ptr — heap allocation
// keeps the listener's address stable across moves of the owning object, so
// the bus's stored reference stays valid.
class TimeListener : public PapoEvent::IPapoEventListener {
public:
  // Subscribes to TimeManager::LastFrameTimeUpdatedEventID on the global bus.
  TimeListener();
  ~TimeListener() override;

  // Refreshes deltaTime_ from Engine::Root::get().getTimeManager(). The
  // LastFrameTimeUpdated event carries no payload.
  void eventCall(void *payload) override;

  float deltaTime() const { return deltaTime_; }
  void setDeltaTime(float dt) { deltaTime_ = dt; }

private:
  float deltaTime_ = 0.0f;
};

class AbstractMoveableObject {
public:
  // Heap-allocates a TimeListener which subscribes to the global event bus
  // for LastFrameTimeUpdated. The listener stays alive for the lifetime of
  // this object (or its move-target, on move).
  AbstractMoveableObject();

  virtual ~AbstractMoveableObject() = default;

  // Move-only. unique_ptr<TimeListener> makes the implicit copy deleted;
  // the user-declared destructor suppresses implicit move generation, so
  // we default the moves back in explicitly. Moving transfers ownership of
  // the heap-allocated listener — its address (and the bus's reference to
  // it) is preserved.
  AbstractMoveableObject(AbstractMoveableObject &&) noexcept = default;
  AbstractMoveableObject &
  operator=(AbstractMoveableObject &&) noexcept = default;
  AbstractMoveableObject(const AbstractMoveableObject &) = delete;
  AbstractMoveableObject &operator=(const AbstractMoveableObject &) = delete;

  virtual glm::vec3 coordinates() const;
  virtual glm::vec3 rotation() const;
  virtual glm::quat rotationQuat() const;
  virtual glm::vec3 scaling() const;

  // Forwards to the owned TimeListener.
  virtual float deltaTime() const;

  // Forwards to the owned TimeListener. Useful for tests or callers that
  // drive time themselves. The bus subscription path overwrites this on
  // the next LastFrameTimeUpdated event.
  virtual void setDeltaTime(float dt);

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
  virtual uint32_t id() const = 0;

private:
  std::unique_ptr<TimeListener> timeListener_;

  // Default to the identity transform so a default-constructed
  // AbstractMoveableObject has a sensible initial state. RenderableObject's
  // ctors immediately overwrite these via the setters.
  glm::vec3 coordinates_{0.0f};

  glm::vec3 rotationRad_{0.0f};
  glm::quat rotationQuat_{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z = identity

  glm::vec3 scaling_{1.0f};
};

// TODO do transform updates lazily, only update rotation coords etc and not the
// transform
class RenderableObject : public AbstractMoveableObject {
public:
  RenderableObject(
      Service::AssetManager::ModelHandle<Service::Model> modelHandle,
      glm::vec3 coordinates, glm::vec3 rotationRad, glm::vec3 scaling,
      uint32_t id = 0);
  RenderableObject(
      Service::AssetManager::ModelHandle<Service::Model> modelHandle,
      glm::vec3 coordinates, glm::quat rotationQuat, glm::vec3 scaling,
      uint32_t id = 0);
  RenderableObject(
      Service::AssetManager::ModelHandle<Service::Model> modelHandle,
      glm::vec3 coordinates, glm::vec3 rotationRad, glm::quat rotationQuat,
      glm::vec3 scaling, uint32_t id = 0);

  // Defaults to identity transform (origin, no rotation, unit scale).
  explicit RenderableObject(
      Service::AssetManager::ModelHandle<Service::Model> modelHandle,
      uint32_t id = 0);

  ~RenderableObject() override = default;

  // Same move-only story as the base: the user-declared dtor suppresses
  // implicit move generation, so default the moves back in explicitly.
  RenderableObject(RenderableObject &&) noexcept = default;
  RenderableObject &operator=(RenderableObject &&) noexcept = default;
  RenderableObject(const RenderableObject &) = delete;
  RenderableObject &operator=(const RenderableObject &) = delete;

  virtual glm::mat4 getTransform() const;

  void setCoordinates(glm::vec3 coordinates) override;
  void setRotation(glm::vec3 rotationRad) override;
  void setRotation(glm::quat rotationQuat) override;
  void setScaling(glm::vec3 scaling) override;

  virtual constexpr inline uint32_t id() const override  {return id_;}

private:
  void updateTransform();

  Service::AssetManager::ModelHandle<Service::Model> modelHandle_;

  glm::mat4 transform_;

  uint32_t id_;
};

} // namespace Runtime
