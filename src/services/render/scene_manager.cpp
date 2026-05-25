#include "services/render/scene_manager.hpp"
#include "glm/geometric.hpp"
#include "services/render/camera.hpp"
#include "services/render/renderable_object.hpp"

namespace Runtime {
Scene::Scene(ICamera &camera) : camera_(camera) {}

StreamingScene::StreamingScene(LayerDescriptorList &&layerDescriptorList,
                               ICamera &camera)
    : layerDescriptorList_(std::move(layerDescriptorList)), Scene(camera) {}

void StreamingScene::renderScene() const {
  for (auto &obj : stableRendered_) {
    // TODO call to render engine
  }
  for (auto &obj : transientRendered_) {
    // TODO call to render engine
  }
}

std::vector<RenderableObject> *
StreamingScene::distanceToRenderVector(std::size_t distance) noexcept {
  switch (layerDescriptorList_.distanceMatcher(distance)) {
  case LayerDescriptorList::STABLE:
    return &stableRendered_;
  case LayerDescriptorList::HOT:
    return &transientRendered_;
  case LayerDescriptorList::WARM:
    return &transientLoaded_;
  case LayerDescriptorList::RAM:
    return &ramLoaded_;
  case LayerDescriptorList::COLD:
    return nullptr;
  }
}

void StreamingScene::addObject(RenderableObject &&model) {
  glm::vec3 coords = camera().coordinates();
  glm::vec3 modelCoords = model.coordinates();

  float distance = glm::distance(coords, modelCoords);
  std::vector<RenderableObject> *v = distanceToRenderVector(distance);
  if (v == nullptr) { // TODO scene graph file usage
  } else {
    v->push_back(); //TODO figure out what to do with the event listener problem. (maybe a class whose only purpose is to be called in event bus)
  }
}

} // namespace Runtime