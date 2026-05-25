#include "services/render/renderable_object.hpp"

#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "services/services.hpp"
#include "services/time_manager.hpp"

namespace Runtime {

// --- TimeListener -----------------------------------------------------------

TimeListener::TimeListener() {
  auto &bus = Engine::Root::get().getEventManager().getEventBus();
  bus.subscribe(Service::TimeManager::LastFrameTimeUpdatedEventID, *this);
}

TimeListener::~TimeListener() {
  auto &bus = Engine::Root::get().getEventManager().getEventBus();
  bus.unsubscribe(Service::TimeManager::LastFrameTimeUpdatedEventID, *this);
}

void TimeListener::eventCall(void * /*payload*/) {
  deltaTime_ = Engine::Root::get().getTimeManager().getLastFramePeriodSeconds();
}

// --- AbstractMoveableObject -------------------------------------------------

AbstractMoveableObject::AbstractMoveableObject()
    : timeListener_(std::make_unique<TimeListener>()) {}

glm::vec3 AbstractMoveableObject::coordinates() const { return coordinates_; }
glm::vec3 AbstractMoveableObject::rotation() const { return rotationRad_; }
glm::quat AbstractMoveableObject::rotationQuat() const { return rotationQuat_; }
glm::vec3 AbstractMoveableObject::scaling() const { return scaling_; }

float AbstractMoveableObject::deltaTime() const {
  return timeListener_->deltaTime();
}

void AbstractMoveableObject::setDeltaTime(float dt) {
  timeListener_->setDeltaTime(dt);
}

void AbstractMoveableObject::setCoordinates(glm::vec3 coordinates) {
  coordinates_ = coordinates;
}

void AbstractMoveableObject::setRotation(glm::vec3 rotationRad) {
  rotationRad_ = rotationRad;
  rotationQuat_ = glm::quat(rotationRad);
}

void AbstractMoveableObject::setRotation(glm::quat rotationQuat) {
  rotationRad_ = glm::eulerAngles(rotationQuat);
  rotationQuat_ = rotationQuat;
}

void AbstractMoveableObject::setScaling(glm::vec3 scaling) {
  scaling_ = scaling;
}

void AbstractMoveableObject::step(glm::vec3 speed) {
  setCoordinates(coordinates() + speed * deltaTime());
}

void AbstractMoveableObject::rotateLocal(glm::vec3 rotationEuler) {
  glm::quat delta(rotationEuler);
  setRotation(glm::normalize(rotationQuat() * delta));
}

void AbstractMoveableObject::rotateWorld(glm::vec3 rotationEuler) {
  glm::quat delta(rotationEuler);
  setRotation(glm::normalize(delta * rotationQuat()));
}

void AbstractMoveableObject::rotateStepWorld(glm::vec3 axis,
                                             float angularSpeed) {
  glm::quat delta =
      glm::angleAxis(angularSpeed * deltaTime(), glm::normalize(axis));
  setRotation(glm::normalize(delta * rotationQuat()));
}

void AbstractMoveableObject::rotateAroundPivot(glm::vec3 pivot,
                                               glm::vec3 rotationEuler) {
  glm::quat delta(rotationEuler);
  setCoordinates(pivot + delta * (coordinates() - pivot));
  setRotation(glm::normalize(delta * rotationQuat()));
}

void AbstractMoveableObject::rotateStepAroundPivot(glm::vec3 pivot,
                                                   glm::vec3 axis,
                                                   float angularSpeed) {
  glm::quat delta =
      glm::angleAxis(angularSpeed * deltaTime(), glm::normalize(axis));
  setCoordinates(pivot + delta * (coordinates() - pivot));
  setRotation(glm::normalize(delta * rotationQuat()));
}

// --- RenderableObject -------------------------------------------------------

// Base members are private with no init-list-friendly base ctor, so we seed
// them through the base setters. updateTransform() is called once at the end
// instead of letting the overridden setters fire it on every call.
// The default-constructed AbstractMoveableObject base creates a TimeListener
// (via unique_ptr) which subscribes to LastFrameTimeUpdated on the global bus.
RenderableObject::RenderableObject(
    Service::AssetManager::ModelHandle<Service::Model> modelHandle,
    glm::vec3 coordinates, glm::vec3 rotationRad, glm::vec3 scaling)
    : modelHandle_(modelHandle) {
  AbstractMoveableObject::setCoordinates(coordinates);
  AbstractMoveableObject::setRotation(rotationRad);
  AbstractMoveableObject::setScaling(scaling);
  updateTransform();
}

RenderableObject::RenderableObject(
    Service::AssetManager::ModelHandle<Service::Model> modelHandle,
    glm::vec3 coordinates, glm::quat rotationQuat, glm::vec3 scaling)
    : modelHandle_(modelHandle) {
  AbstractMoveableObject::setCoordinates(coordinates);
  AbstractMoveableObject::setRotation(rotationQuat);
  AbstractMoveableObject::setScaling(scaling);
  updateTransform();
}

// Caller passes both Euler and quat. Treat the quat as authoritative (it's
// the form actually used to build the transform); the Euler input is
// effectively ignored beyond what the quat encodes. rotation() will return
// glm::eulerAngles(rotationQuat).
RenderableObject::RenderableObject(
    Service::AssetManager::ModelHandle<Service::Model> modelHandle,
    glm::vec3 coordinates, glm::vec3 /*rotationRad*/, glm::quat rotationQuat,
    glm::vec3 scaling)
    : modelHandle_(modelHandle) {
  AbstractMoveableObject::setCoordinates(coordinates);
  AbstractMoveableObject::setRotation(rotationQuat);
  AbstractMoveableObject::setScaling(scaling);
  updateTransform();
}

// Defaults to the identity transform: origin, no rotation, unit scale.
RenderableObject::RenderableObject(
    Service::AssetManager::ModelHandle<Service::Model> modelHandle)
    : RenderableObject(modelHandle, glm::vec3(0.0f), glm::vec3(0.0f),
                       glm::vec3(1.0f)) {}

glm::mat4 RenderableObject::getTransform() const { return transform_; }

void RenderableObject::setCoordinates(glm::vec3 coordinates) {
  AbstractMoveableObject::setCoordinates(coordinates);
  updateTransform();
}

void RenderableObject::setRotation(glm::vec3 rotationRad) {
  AbstractMoveableObject::setRotation(rotationRad);
  updateTransform();
}

void RenderableObject::setRotation(glm::quat rotationQuat) {
  AbstractMoveableObject::setRotation(rotationQuat);
  updateTransform();
}

void RenderableObject::setScaling(glm::vec3 scaling) {
  AbstractMoveableObject::setScaling(scaling);
  updateTransform();
}

void RenderableObject::updateTransform() {
  glm::mat4 T = glm::translate(glm::mat4(1.0f), coordinates());
  glm::mat4 R = glm::mat4_cast(rotationQuat());
  glm::mat4 S = glm::scale(glm::mat4(1.0f), scaling());
  transform_ = T * R * S;
}

} // namespace Runtime
