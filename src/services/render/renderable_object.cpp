#include "services/render/renderable_object.hpp"

#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Runtime {

// --- AbstractMoveableObject -------------------------------------------------

glm::vec3 AbstractMoveableObject::coordinates() const { return coordinates_; }
glm::vec3 AbstractMoveableObject::rotation() const { return rotationRad_; }
glm::quat AbstractMoveableObject::rotationQuat() const { return rotationQuat_; }
glm::vec3 AbstractMoveableObject::scaling() const { return scaling_; }

void AbstractMoveableObject::rotateLocal(glm::vec3 rotationEuler) {
  glm::quat delta(rotationEuler);
  rotationQuat_ = glm::normalize(rotationQuat_ * delta);
  setRotation(rotationQuat_);
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

// TODO implement and test — empty stub so the vtable links.
void AbstractMoveableObject::step(glm::vec3 /*speed*/) {}

// --- RenderableObject -------------------------------------------------------

// Base members are private with no init-list-friendly base ctor, so we seed
// them through the base setters. updateTransform() is called once at the end
// instead of letting the overridden setters fire it on every call.
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
