#include "services/render/scene_manager.hpp"
#include "glm/geometric.hpp"
#include "services/render/camera.hpp"
#include "services/render/renderable_object.hpp"
#include <cstdint>
#include <vector>

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

std::vector<RenderableObject> *StreamingScene::descriptorNameToRenderVector(
    StreamingScene::LayerDescriptorList::DescriptorNames layer) noexcept {
  using LayerDescriptorList = StreamingScene::LayerDescriptorList;
  switch (layer) {
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

std::vector<RenderableObject> *
StreamingScene::distanceToRenderVector(std::size_t distance) noexcept {
  return descriptorNameToRenderVector(
      layerDescriptorList_.distanceMatcher(distance));
}

void StreamingScene::addObject(RenderableObject &&model) {
  glm::vec3 coords = camera().coordinates();
  glm::vec3 modelCoords = model.coordinates();

  float distance = glm::distance(coords, modelCoords);
  std::vector<RenderableObject> *v = distanceToRenderVector(distance);
  if (v != nullptr) {
    v->push_back(std::move(model));
    return;
  }

  // TODO scene graph file push obj usage
}

void StreamingScene::removeObject(uint32_t objectId,
                                  LayerDescriptorList::DescriptorNames layer) {
  if (auto vec = distanceToRenderVector(layer); vec != nullptr) {
    for (int i = 0; i < vec->size(); i++) {
      auto &elem = vec->at(i);
      if (elem.id() == objectId) {
        vec->at(i) = std::move(vec->back());
        vec->pop_back();
      }
    }
    return;
  }
  // TODO filesystem scene graph work
}

void StreamingScene::removeObject(uint32_t objectId) {
  std::vector<std::vector<RenderableObject> *> vecs(
      {&stableRendered_, &transientRendered_, &transientLoaded_, &ramLoaded_});

  for (auto vec : vecs) {
    for (int i = 0; i < vec->size(); i++) {
      auto &elem = vec->at(i);
      if (elem.id() == objectId) {
        vec->at(i) = std::move(vec->back());
        vec->pop_back();
        return;
      }
    }
  }

  // TODO filesystem scene graph work
}

} // namespace Runtime