#include "services/render/renderable_object.hpp"

namespace Service {
RenderableObject::RenderableObject(AssetManager::ModelHandle<Model> modelHandle,
                                   glm::vec3 coordinates, glm::vec3 rotationRad,
                                   glm::vec3 scaling)
    : modelHandle_(modelHandle), coordinates_(coordinates),
      rotationRad_(rotationRad), rotationQuat_(rotationRad), scaling_(scaling) {
  updateTransform();
}
RenderableObject::RenderableObject(AssetManager::ModelHandle<Model> modelHandle,
                                   glm::vec3 coordinates,
                                   glm::quat rotationQuat, glm::vec3 scaling)
    : modelHandle_(modelHandle), coordinates_(coordinates),
      rotationRad_(glm::eulerAngles(rotationQuat)), rotationQuat_(rotationQuat),
      scaling_(scaling) {
  updateTransform();
}
RenderableObject::RenderableObject(AssetManager::ModelHandle<Model> modelHandle,
                                   glm::vec3 coordinates, glm::vec3 rotationRad,
                                   glm::quat rotationQuat, glm::vec3 scaling)
    : modelHandle_(modelHandle), coordinates_(coordinates),
      rotationRad_(rotationRad), rotationQuat_(rotationQuat),
      scaling_(scaling) {
  updateTransform();
}
} // namespace Service